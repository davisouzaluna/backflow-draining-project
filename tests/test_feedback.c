#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include "config.h"
#include "adaptador.h"
#include "feedback.h"

static int falhas;

#define CHECK(cond, msg)                                              \
    do {                                                              \
        if (!(cond)) {                                                \
            printf("FALHOU: %s (linha %d)\n", msg, __LINE__);         \
            falhas++;                                                 \
        }                                                             \
    } while (0)

static LeituraVirtual leitura(unsigned long seq, double chuva, double mare, int alerta)
{
    LeituraVirtual l;

    memset(&l, 0, sizeof(l));
    l.boot_id = 1000;
    l.seq = seq;
    l.epoch = time(NULL);
    l.chuva_media = chuva;
    l.analise.mare_m = mare;
    l.analise.alerta = alerta;
    l.analise.risco = alerta ? RISCO_OCORRENDO : RISCO_NENHUM;
    return l;
}

static int enviar(const char *fmt, const char *user, unsigned long seq, const char *houve)
{
    char buf[256];

    snprintf(buf, sizeof(buf), fmt, user, seq, houve);
    return feedback_processar(buf, strlen(buf));
}

int main(void)
{
    const char *resp = "{\"user\":\"%s\",\"ref_boot\":1000,\"ref_seq\":%lu,\"houve\":%s}";
    LeituraVirtual a = leitura(1, 8.0, 2.4, 1);
    LeituraVirtual b = leitura(2, 3.0, 1.8, 0);
    Limiares antes, depois;
    int i, aceitos = 0;
    char u[16];

    mkdir("/tmp/bf_test_data", 0777);
    remove(ARQUIVO_FEEDBACK);
    remove(ARQUIVO_ESTADO_LIMIARES);
    adaptador_iniciar();

    feedback_registrar_leitura(&a);
    feedback_registrar_leitura(&b);
    CHECK(a.pergunta == 1 && strcmp(a.pergunta_motivo, "alerta") == 0, "alerta gera pergunta");
    CHECK(b.pergunta == 0, "intervalo minimo entre perguntas");

    antes = adaptador_limiares();
    CHECK(enviar(resp, "u1", 1, "false") == 1, "FP aceito");
    depois = adaptador_limiares();
    CHECK(depois.chuva_mm > antes.chuva_mm || depois.mare_m > antes.mare_m, "FP sobe limiar");
    CHECK(enviar(resp, "u1", 1, "false") == 0, "duplicado rejeitado");
    CHECK(enviar(resp, "u1", 99, "true") == 0, "referencia desconhecida rejeitada");
    CHECK(feedback_processar("lixo", 4) == 0, "json invalido rejeitado");
    CHECK(feedback_processar("{\"user\":\"a,b\",\"houve\":true}", 28) == 0, "usuario com virgula");

    antes = adaptador_limiares();
    CHECK(enviar(resp, "u2", 2, "true") == 1, "FN aceito");
    depois = adaptador_limiares();
    CHECK(depois.chuva_mm < antes.chuva_mm || depois.mare_m < antes.mare_m, "FN desce limiar");
    CHECK(feedback_processar("{\"user\":\"u3\",\"tipo\":\"espontaneo\",\"houve\":false}", 52) == 0,
          "espontaneo negativo ignorado");
    CHECK(feedback_processar("{\"user\":\"u3\",\"tipo\":\"espontaneo\",\"houve\":true}", 51) == 1,
          "espontaneo positivo aceito");

    for (i = 0; i < 20; i++) {
        LeituraVirtual x = leitura(100 + i, 8.0, 2.4, 1);

        feedback_registrar_leitura(&x);
        snprintf(u, sizeof(u), "lote%d", i);
        aceitos += enviar(resp, u, 100 + i, "false");
    }
    CHECK(aceitos < 20, "limite global de atualizacoes por hora");

    {
        EstatisticaFeedback e = feedback_estatistica();

        CHECK(e.recebidos == e.aceitos + e.rejeitados, "contagens consistentes");
    }
    if (falhas == 0)
        printf("OK: feedback\n");
    return falhas != 0;
}
