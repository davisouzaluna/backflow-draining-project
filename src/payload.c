#include <time.h>
#include <cjson/cJSON.h>
#include "analisador.h"
#include "mare_tabua.h"
#include "payload.h"
#include "adaptador.h"
#include "feedback.h"

static void add_meta(cJSON *pai, const char *nome, const Avaliacao *a)
{
    cJSON *o = cJSON_AddObjectToObject(pai, nome);

    cJSON_AddNumberToObject(o, "valor", a->valor);
    cJSON_AddNumberToObject(o, "meta", a->meta);
    cJSON_AddStringToObject(o, "resultado", analisador_nome_comparacao(a->resultado));
}

char *payload_serializar(const LeituraVirtual *l)
{
    cJSON *raiz = cJSON_CreateObject();
    cJSON *o;
    char *texto;

    if (raiz == NULL)
        return NULL;

    {
        struct timespec ts;
        timespec_get(&ts, TIME_UTC);
        cJSON_AddNumberToObject(raiz, "boot_id", (double)l->boot_id);
        cJSON_AddNumberToObject(raiz, "seq", (double)l->seq);
        cJSON_AddNumberToObject(raiz, "pub_ms", (double)ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6);
    }
    cJSON_AddStringToObject(raiz, "timestamp", l->timestamp);

    o = cJSON_AddObjectToObject(raiz, "chuva");
    cJSON_AddBoolToObject(o, "disponivel", l->chuva.disponivel);
    cJSON_AddNumberToObject(o, "media_mm", l->chuva_media);
    cJSON_AddNumberToObject(o, "maior_mm", l->chuva.maior_chuva);
    cJSON_AddNumberToObject(o, "estacoes", l->chuva.total_estacoes);
    cJSON_AddStringToObject(o, "ultima_leitura", l->chuva.ultima_leitura);
    cJSON_AddNumberToObject(o, "idade_dado_s", l->chuva.idade_dado_s);
    cJSON_AddNumberToObject(o, "fetch_ms", l->chuva.fetch_ms);
    cJSON_AddNumberToObject(o, "http_code", (double)l->chuva.http_code);

    o = cJSON_AddObjectToObject(raiz, "mare");
    cJSON_AddBoolToObject(o, "disponivel", l->mare.disponivel);
    cJSON_AddStringToObject(o, "evento_data", l->mare.data);
    cJSON_AddStringToObject(o, "evento_hora", l->mare.hora);
    cJSON_AddNumberToObject(o, "altura_m", l->mare.altura);
    cJSON_AddNumberToObject(o, "altura_interp_m", mare_altura_efetiva(&l->mare));
    cJSON_AddNumberToObject(o, "min_dia_m", l->mare.menor_altura_dia);
    cJSON_AddNumberToObject(o, "max_dia_m", l->mare.maior_altura_dia);
    cJSON_AddNumberToObject(o, "posicao", mare_posicao_relativa(&l->mare));
    cJSON_AddNumberToObject(o, "dif_min", (double)l->mare.diferenca_minutos);

    o = cJSON_AddObjectToObject(raiz, "drenagem");
    cJSON_AddNumberToObject(o, "bocas_lobo", l->drenagem.bocas_lobo);
    cJSON_AddNumberToObject(o, "pocos_visita", l->drenagem.pocos_visita);
    cJSON_AddNumberToObject(o, "caixas_gaveta", l->drenagem.caixas_gaveta);
    cJSON_AddNumberToObject(o, "total", l->drenagem.total_elementos);

    o = cJSON_AddObjectToObject(raiz, "metas");
    add_meta(o, "chuva_margem", &l->analise.chuva_margem);
    add_meta(o, "chuva_limiar", &l->analise.chuva_limiar);
    add_meta(o, "mare_margem", &l->analise.mare_margem);
    add_meta(o, "mare_limiar", &l->analise.mare_limiar);

    o = cJSON_AddObjectToObject(raiz, "limiares");
    cJSON_AddNumberToObject(o, "chuva_mm", l->analise.limiares.chuva_mm);
    cJSON_AddNumberToObject(o, "mare_m", l->analise.limiares.mare_m);

    o = cJSON_AddObjectToObject(raiz, "risco");
    cJSON_AddNumberToObject(o, "codigo", l->analise.risco);
    cJSON_AddStringToObject(o, "nome", analisador_nome_risco(l->analise.risco));
    cJSON_AddBoolToObject(o, "alerta", l->analise.alerta);

    o = cJSON_AddObjectToObject(raiz, "pergunta");
    cJSON_AddBoolToObject(o, "ativa", l->pergunta);
    cJSON_AddStringToObject(o, "motivo", l->pergunta_motivo);

    {
        ContagemFeedback c = adaptador_contagens();
        EstatisticaFeedback e = feedback_estatistica();

        o = cJSON_AddObjectToObject(raiz, "feedback");
        cJSON_AddNumberToObject(o, "tp", (double)c.tp);
        cJSON_AddNumberToObject(o, "fp", (double)c.fp);
        cJSON_AddNumberToObject(o, "fn", (double)c.fn);
        cJSON_AddNumberToObject(o, "tn", (double)c.tn);
        cJSON_AddNumberToObject(o, "atualizacoes", (double)c.atualizacoes);
        cJSON_AddNumberToObject(o, "fn_ignorados", (double)c.fn_ignorados);
        cJSON_AddNumberToObject(o, "recebidos", (double)e.recebidos);
        cJSON_AddNumberToObject(o, "rejeitados", (double)e.rejeitados);
    }

    texto = cJSON_PrintUnformatted(raiz);
    cJSON_Delete(raiz);
    return texto;
}
