#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <cjson/cJSON.h>
#include "config.h"
#include "adaptador.h"
#include "feedback.h"

#define ANEL 64
#define USUARIOS 128
#define VISTOS 256
#define NOME_MAX 64

typedef struct {
    long long boot;
    unsigned long seq;
    time_t epoch;
    double chuva_mm;
    double mare_m;
    int alerta;
    int valido;
} Contexto;

typedef struct {
    char nome[NOME_MAX];
    time_t janela;
    int usados;
} Usuario;

typedef struct {
    char nome[NOME_MAX];
    long long boot;
    unsigned long seq;
} Visto;

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static Contexto anel[ANEL];
static size_t cabeca;
static Usuario usuarios[USUARIOS];
static Visto vistos[VISTOS];
static size_t prox_visto;
static time_t ultima_pergunta;
static time_t janela_global;
static int usados_global;
static int semeado;
static EstatisticaFeedback est;

static double sorteio(void)
{
    if (!semeado) {
        srand(FEEDBACK_SEMENTE ? (unsigned)FEEDBACK_SEMENTE : (unsigned)time(NULL));
        semeado = 1;
    }
    return rand() / ((double)RAND_MAX + 1.0);
}

void feedback_registrar_leitura(LeituraVirtual *l)
{
    Contexto *c;
    const char *motivo = NULL;

    l->pergunta = 0;
    l->pergunta_motivo[0] = '\0';

    pthread_mutex_lock(&trava);
    c = &anel[cabeca];
    cabeca = (cabeca + 1) % ANEL;
    c->boot = l->boot_id;
    c->seq = l->seq;
    c->epoch = l->epoch;
    c->chuva_mm = l->chuva_media;
    c->mare_m = l->analise.mare_m;
    c->alerta = l->analise.alerta;
    c->valido = l->analise.risco != RISCO_INDETERMINADO;

#if FEEDBACK_HABILITADO
    if (c->valido && difftime(l->epoch, ultima_pergunta) >= PERGUNTA_INTERVALO_MIN_S) {
        if (l->analise.alerta)
            motivo = "alerta";
        else if (l->analise.risco == RISCO_POTENCIAL && sorteio() < 5.0 * PERGUNTA_EPSILON)
            motivo = "margem";
        else if (sorteio() < PERGUNTA_EPSILON)
            motivo = "explora";
    }
    if (motivo != NULL) {
        l->pergunta = 1;
        snprintf(l->pergunta_motivo, sizeof(l->pergunta_motivo), "%s", motivo);
        ultima_pergunta = l->epoch;
    }
#endif
    pthread_mutex_unlock(&trava);
}

static const Contexto *buscar(long long boot, unsigned long seq)
{
    size_t i;

    for (i = 0; i < ANEL; i++)
        if (anel[i].epoch != 0 && anel[i].boot == boot && anel[i].seq == seq)
            return &anel[i];
    return NULL;
}

static const Contexto *ultimo(void)
{
    return &anel[(cabeca + ANEL - 1) % ANEL];
}

static void log_feedback(time_t agora, const char *usuario, const char *origem,
                         const Contexto *c, int houve, const ResultadoFeedback *r,
                         double peso, const char *rejeicao)
{
    FILE *f = fopen(ARQUIVO_FEEDBACK, "a+");

    if (f == NULL)
        return;
    fseek(f, 0, SEEK_END);
    if (ftell(f) == 0)
        fputs("epoch,usuario,origem,ref_boot,ref_seq,houve,ctx_chuva_mm,"
              "ctx_mare_m,ctx_alerta,tipo,lim_chuva_antes,lim_mare_antes,"
              "lim_chuva_depois,lim_mare_depois,peso,aceito,rejeicao\n", f);
    fprintf(f, "%lld,%s,%s,", (long long)agora, usuario, origem);
    if (c != NULL)
        fprintf(f, "%lld,%lu,%d,%.4f,%.4f,%d,", c->boot, c->seq, houve,
                c->chuva_mm, c->mare_m, c->alerta);
    else
        fprintf(f, ",,%d,,,,", houve);
    if (r != NULL)
        fprintf(f, "%s,%.4f,%.4f,%.4f,%.4f,%.2f,1,\n", adaptador_nome_tipo(r->tipo),
                r->antes.chuva_mm, r->antes.mare_m, r->depois.chuva_mm,
                r->depois.mare_m, peso);
    else
        fprintf(f, ",,,,,%.2f,0,%s\n", peso, rejeicao);
    fclose(f);
}

static int nome_valido(const char *s)
{
    size_t n = strlen(s);
    size_t i;

    if (n == 0 || n >= NOME_MAX)
        return 0;
    for (i = 0; i < n; i++)
        if (s[i] == ',' || s[i] == '"' || s[i] == '\n' || s[i] == '\r' ||
            (unsigned char)s[i] < 32)
            return 0;
    return 1;
}

static Usuario *achar_usuario(const char *nome, time_t agora)
{
    size_t i, livre = USUARIOS;

    for (i = 0; i < USUARIOS; i++) {
        if (usuarios[i].nome[0] == '\0') {
            if (livre == USUARIOS)
                livre = i;
        } else if (strcmp(usuarios[i].nome, nome) == 0) {
            return &usuarios[i];
        }
    }
    if (livre == USUARIOS) {
        for (i = 0; i < USUARIOS; i++)
            if (difftime(agora, usuarios[i].janela) >= 3600) {
                livre = i;
                break;
            }
    }
    if (livre == USUARIOS)
        return NULL;
    memset(&usuarios[livre], 0, sizeof(usuarios[livre]));
    snprintf(usuarios[livre].nome, NOME_MAX, "%s", nome);
    usuarios[livre].janela = agora;
    return &usuarios[livre];
}

static int ja_visto(const char *nome, long long boot, unsigned long seq)
{
    size_t i;

    for (i = 0; i < VISTOS; i++)
        if (vistos[i].nome[0] && vistos[i].boot == boot && vistos[i].seq == seq &&
            strcmp(vistos[i].nome, nome) == 0)
            return 1;
    return 0;
}

int feedback_processar(const char *json, size_t n)
{
    cJSON *raiz = cJSON_ParseWithLength(json, n);
    const cJSON *u, *h, *b, *s, *t;
    char usuario[NOME_MAX] = "?";
    const char *origem = "resposta";
    const char *rej = NULL;
    const Contexto *c = NULL;
    Contexto copia;
    ResultadoFeedback res;
    time_t agora = time(NULL);
    int houve = 0, aceito = 0;
    Usuario *ur;

    pthread_mutex_lock(&trava);
    est.recebidos++;

    if (raiz == NULL) {
        rej = "json_invalido";
        goto fim;
    }
    u = cJSON_GetObjectItemCaseSensitive(raiz, "user");
    h = cJSON_GetObjectItemCaseSensitive(raiz, "houve");
    b = cJSON_GetObjectItemCaseSensitive(raiz, "ref_boot");
    s = cJSON_GetObjectItemCaseSensitive(raiz, "ref_seq");
    t = cJSON_GetObjectItemCaseSensitive(raiz, "tipo");
    if (!cJSON_IsString(u) || !nome_valido(u->valuestring)) {
        rej = "usuario_invalido";
        goto fim;
    }
    snprintf(usuario, sizeof(usuario), "%s", u->valuestring);
    if (!cJSON_IsBool(h)) {
        rej = "houve_invalido";
        goto fim;
    }
    houve = cJSON_IsTrue(h);
    if (cJSON_IsString(t) && strcmp(t->valuestring, "espontaneo") == 0)
        origem = "espontaneo";

    if (strcmp(origem, "espontaneo") == 0) {
        if (!houve) {
            rej = "espontaneo_negativo";
            goto fim;
        }
        c = ultimo();
        if (c->epoch == 0) {
            rej = "sem_contexto";
            goto fim;
        }
    } else {
        if (!cJSON_IsNumber(b) || !cJSON_IsNumber(s)) {
            rej = "referencia_ausente";
            goto fim;
        }
        c = buscar((long long)b->valuedouble, (unsigned long)s->valuedouble);
        if (c == NULL) {
            rej = "referencia_desconhecida";
            goto fim;
        }
    }
    if (difftime(agora, c->epoch) > FEEDBACK_VALIDADE_S) {
        rej = "expirado";
        goto fim;
    }
    if (!c->valido) {
        rej = "contexto_indeterminado";
        goto fim;
    }
    if (strcmp(origem, "resposta") == 0 && ja_visto(usuario, c->boot, c->seq)) {
        rej = "duplicado";
        goto fim;
    }
    ur = achar_usuario(usuario, agora);
    if (ur == NULL) {
        rej = "tabela_cheia";
        goto fim;
    }
    if (difftime(agora, ur->janela) >= 3600) {
        ur->janela = agora;
        ur->usados = 0;
    }
    if (ur->usados >= FEEDBACK_MAX_POR_USUARIO_HORA) {
        rej = "limite_usuario";
        goto fim;
    }
    if (difftime(agora, janela_global) >= 3600) {
        janela_global = agora;
        usados_global = 0;
    }
    if (usados_global >= FEEDBACK_MAX_ATUALIZACOES_HORA) {
        rej = "limite_global";
        goto fim;
    }
    ur->usados++;
    usados_global++;
    copia = *c;
    c = &copia;
    snprintf(vistos[prox_visto].nome, NOME_MAX, "%s", usuario);
    vistos[prox_visto].boot = c->boot;
    vistos[prox_visto].seq = c->seq;
    prox_visto = (prox_visto + 1) % VISTOS;
    aceito = 1;

fim:
    if (aceito) {
        est.aceitos++;
        pthread_mutex_unlock(&trava);
        res = adaptador_aplicar(c->chuva_mm, c->mare_m, c->alerta, houve, 1.0);
        pthread_mutex_lock(&trava);
        log_feedback(agora, usuario, origem, c, houve, &res, 1.0, NULL);
    } else {
        est.rejeitados++;
        log_feedback(agora, usuario, origem, c, houve, NULL, 0.0, rej);
    }
    pthread_mutex_unlock(&trava);
    cJSON_Delete(raiz);
    return aceito;
}

EstatisticaFeedback feedback_estatistica(void)
{
    EstatisticaFeedback e;

    pthread_mutex_lock(&trava);
    e = est;
    pthread_mutex_unlock(&trava);
    return e;
}
