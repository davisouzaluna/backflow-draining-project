#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "config.h"
#include "adaptador.h"
#include "analisador.h"
#include "mare_tabua.h"
#include "tempo_util.h"

static int falhas;

#define CHECK(cond, msg)                                              \
    do {                                                              \
        if (!(cond)) {                                                \
            printf("FALHOU: %s (linha %d)\n", msg, __LINE__);         \
            falhas++;                                                 \
        }                                                             \
    } while (0)

static int alerta_em(Limiares l, double r, double h)
{
    return r >= l.chuva_mm && h >= l.mare_m;
}

static void test_tp_tn_nao_mudam(void)
{
    Limiares l = {5.0, 2.0};
    Limiares a = adaptador_passo(l, 8.0, 2.4, 1, 1, 1.0);
    Limiares b = adaptador_passo(l, 1.0, 1.0, 0, 0, 1.0);

    CHECK(a.chuva_mm == l.chuva_mm && a.mare_m == l.mare_m, "TP nao altera");
    CHECK(b.chuva_mm == l.chuva_mm && b.mare_m == l.mare_m, "TN nao altera");
}

static void test_fp_converge(void)
{
    Limiares l = {LIMIAR_CHUVA_INICIAL_MM, LIMIAR_MARE_INICIAL_M};
    double r = 7.0, h = 2.3;
    int i;

    CHECK(alerta_em(l, r, h), "contexto alerta no inicio");
    for (i = 0; i < 100 && alerta_em(l, r, h); i++) {
        Limiares n = adaptador_passo(l, r, h, 1, 0, 1.0);

        CHECK(n.chuva_mm >= l.chuva_mm && n.mare_m >= l.mare_m, "FP nunca reduz");
        l = n;
    }
    CHECK(!alerta_em(l, r, h), "FP repetido cessa o alerta");
    CHECK(i < 100, "FP converge em poucos passos");
}

static void test_fn_converge(void)
{
    Limiares l = {LIMIAR_CHUVA_INICIAL_MM, LIMIAR_MARE_INICIAL_M};
    double r = 3.0, h = 1.8;
    int i;

    CHECK(!alerta_em(l, r, h), "contexto sem alerta no inicio");
    for (i = 0; i < 200 && !alerta_em(l, r, h); i++) {
        Limiares n = adaptador_passo(l, r, h, 0, 1, 1.0);

        CHECK(n.chuva_mm <= l.chuva_mm && n.mare_m <= l.mare_m, "FN nunca aumenta");
        l = n;
    }
    CHECK(alerta_em(l, r, h), "FN repetido produz alerta");
}

static void test_limites(void)
{
    Limiares l = {LIMIAR_CHUVA_MAX_MM, LIMIAR_MARE_MAX_M};
    Limiares n;
    int i;

    for (i = 0; i < 50; i++)
        l = adaptador_passo(l, 60.0, 3.0, 1, 0, 1.0);
    CHECK(l.chuva_mm <= LIMIAR_CHUVA_MAX_MM && l.mare_m <= LIMIAR_MARE_MAX_M,
          "respeita limites superiores");
    l.chuva_mm = LIMIAR_CHUVA_MIN_MM;
    l.mare_m = LIMIAR_MARE_MIN_M;
    n = adaptador_passo(l, -5.0, 0.0, 0, 1, 1.0);
    CHECK(n.chuva_mm >= LIMIAR_CHUVA_MIN_MM && n.mare_m >= LIMIAR_MARE_MIN_M,
          "respeita limites inferiores");
}

static void test_peso_zero(void)
{
    Limiares l = {5.0, 2.0};
    Limiares n = adaptador_passo(l, 7.0, 2.3, 1, 0, 0.0);

    CHECK(n.chuva_mm == l.chuva_mm && n.mare_m == l.mare_m, "peso 0 nao altera");
}

static void test_regiao_de_confianca(void)
{
    Limiares l = {20.0, 2.0};
    Limiares n = adaptador_passo(l, 0.0, 1.2, 0, 1, 1.0);

    CHECK(n.chuva_mm == l.chuva_mm && n.mare_m == l.mare_m,
          "alagamento longe da fronteira (inexplicado) nao altera limiares");
}

static void test_margem_simetrica(void)
{
    Limiares l = {10.0, 2.0};
    Limiares tp = adaptador_passo(l, 10.2, 2.02, 1, 1, 1.0);
    Limiares tn = adaptador_passo(l, 9.8, 1.98, 0, 0, 1.0);

    CHECK(tp.chuva_mm < l.chuva_mm || tp.mare_m < l.mare_m, "TP dentro da margem relaxa limiar");
    CHECK(tn.chuva_mm > l.chuva_mm || tn.mare_m > l.mare_m, "TN dentro da margem endurece limiar");
}

static void test_mare_interpolada(void)
{
    const char *arq = "/tmp/test_mare_interp.csv";
    FILE *f = fopen(arq, "w");
    DadosMare m;
    time_t t0, t;

    if (f == NULL) {
        printf("FALHOU: nao criou arquivo temporario\n");
        falhas++;
        return;
    }
    fputs("data,hora,altura\n2026-03-10,06:00,2.50\n2026-03-10,12:00,0.20\n", f);
    fclose(f);
    tempo_epoch("2026-03-10", "06:00", &t0);

    t = t0 + 3 * 3600;
    CHECK(mare_tabua_obter(arq, t, &m), "meio do intervalo disponivel");
    CHECK(m.interp_ok, "interpolacao ok");
    CHECK(fabs(mare_altura_efetiva(&m) - 1.35) < 0.01, "ponto medio = media");

    t = t0 + 3600;
    mare_tabua_obter(arq, t, &m);
    CHECK(mare_altura_efetiva(&m) > 2.0 && mare_altura_efetiva(&m) < 2.5,
          "1h apos preamar ainda alta (cosseno)");

    t = t0;
    mare_tabua_obter(arq, t, &m);
    CHECK(fabs(mare_altura_efetiva(&m) - 2.50) < 0.01, "no evento = altura do evento");

    t = t0 + 40 * 3600;
    CHECK(!mare_tabua_obter(arq, t, &m), "longe dos eventos = indisponivel");
    remove(arq);
}

static void test_analisador(void)
{
    DadosChuva c;
    DadosMare m;
    Limiares l = {5.0, 2.0};
    Analise a;

    memset(&c, 0, sizeof(c));
    memset(&m, 0, sizeof(m));
    m.disponivel = 1;
    m.altura = 2.2;
    c.disponivel = 0;

    a = analisador_executar(&c, &m, l);
    CHECK(a.risco == RISCO_INDETERMINADO, "sem chuva e limiar>0: indeterminado");
    l.chuva_mm = 0.0;
    a = analisador_executar(&c, &m, l);
    CHECK(a.risco == RISCO_OCORRENDO && a.alerta, "modo so-mare com limiar de chuva 0");
    m.altura = 1.0;
    a = analisador_executar(&c, &m, l);
    CHECK(a.risco == RISCO_NENHUM, "mare baixa: sem risco");
}

int main(void)
{
    test_tp_tn_nao_mudam();
    test_fp_converge();
    test_fn_converge();
    test_limites();
    test_peso_zero();
    test_regiao_de_confianca();
    test_margem_simetrica();
    test_mare_interpolada();
    test_analisador();
    if (falhas == 0)
        printf("OK: todos os testes passaram\n");
    return falhas != 0;
}
