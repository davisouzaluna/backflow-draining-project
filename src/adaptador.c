#include <pthread.h>
#include <stdio.h>
#include "config.h"
#include "adaptador.h"

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static Limiares atual = {LIMIAR_CHUVA_INICIAL_MM, LIMIAR_MARE_INICIAL_M};
static ContagemFeedback cont;

static double limitar(double v, double lo, double hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static Limiares dentro_dos_limites(Limiares l)
{
    l.chuva_mm = limitar(l.chuva_mm, LIMIAR_CHUVA_MIN_MM, LIMIAR_CHUVA_MAX_MM);
    l.mare_m = limitar(l.mare_m, LIMIAR_MARE_MIN_M, LIMIAR_MARE_MAX_M);
    return l;
}

TipoFeedback adaptador_classificar(int alerta, int houve)
{
    if (alerta)
        return houve ? FB_TP : FB_FP;
    return houve ? FB_FN : FB_TN;
}

const char *adaptador_nome_tipo(TipoFeedback t)
{
    switch (t) {
    case FB_TP: return "TP";
    case FB_FP: return "FP";
    case FB_FN: return "FN";
    default:    return "TN";
    }
}

Limiares adaptador_passo(Limiares l, double r, double h, int alerta, int houve,
                         double peso)
{
    double eta = limitar(ADAPT_TAXA * peso, 0.0, 1.0);
    double sr = LIMIAR_CHUVA_MAX_MM - LIMIAR_CHUVA_MIN_MM;
    double sh = LIMIAR_MARE_MAX_M - LIMIAR_MARE_MIN_M;
    double gr = (r - l.chuva_mm) / sr;
    double gh = (h - l.mare_m) / sh;
    double s_and = gr < gh ? gr : gh;

    (void)alerta;
    if (houve) {
        if (s_and < ADAPT_MARGEM) {
            double falta_r = ADAPT_MARGEM - gr > 0.0 ? ADAPT_MARGEM - gr : 0.0;
            double falta_h = ADAPT_MARGEM - gh > 0.0 ? ADAPT_MARGEM - gh : 0.0;

            if (falta_r <= ADAPT_RAIO_CONFIANCA && falta_h <= ADAPT_RAIO_CONFIANCA) {
                double e = limitar(eta * (s_and < 0.0 ? ADAPT_PESO_PERDA : 1.0), 0.0, 1.0);

                l.chuva_mm -= e * falta_r * sr;
                l.mare_m -= e * falta_h * sh;
            }
        }
    } else if (s_and > -ADAPT_MARGEM) {
        if (gr <= gh)
            l.chuva_mm += eta * (gr + ADAPT_MARGEM) * sr;
        else
            l.mare_m += eta * (gh + ADAPT_MARGEM) * sh;
    }
    return dentro_dos_limites(l);
}

static void salvar_estado(void)
{
    char tmp[300];
    FILE *f;

    snprintf(tmp, sizeof(tmp), "%s.tmp", ARQUIVO_ESTADO_LIMIARES);
    f = fopen(tmp, "w");
    if (f == NULL)
        return;
    fprintf(f, "%.6f,%.6f,%lu,%lu,%lu,%lu,%lu,%lu\n", atual.chuva_mm, atual.mare_m,
            cont.tp, cont.fp, cont.fn, cont.tn, cont.atualizacoes, cont.fn_ignorados);
    fclose(f);
    rename(tmp, ARQUIVO_ESTADO_LIMIARES);
}

void adaptador_iniciar(void)
{
    FILE *f = fopen(ARQUIVO_ESTADO_LIMIARES, "r");
    Limiares l;
    ContagemFeedback c = {0};
    int lidos;

    pthread_mutex_lock(&trava);
    if (f != NULL) {
        lidos = fscanf(f, "%lf,%lf,%lu,%lu,%lu,%lu,%lu,%lu", &l.chuva_mm, &l.mare_m,
                       &c.tp, &c.fp, &c.fn, &c.tn, &c.atualizacoes, &c.fn_ignorados);
        if (lidos >= 7) {
            atual = dentro_dos_limites(l);
            cont = c;
        }
        fclose(f);
    }
    pthread_mutex_unlock(&trava);
}

Limiares adaptador_limiares(void)
{
    Limiares l;

    pthread_mutex_lock(&trava);
    l = atual;
    pthread_mutex_unlock(&trava);
    return l;
}

ContagemFeedback adaptador_contagens(void)
{
    ContagemFeedback c;

    pthread_mutex_lock(&trava);
    c = cont;
    pthread_mutex_unlock(&trava);
    return c;
}

ResultadoFeedback adaptador_aplicar(double r, double h, int alerta, int houve,
                                    double peso)
{
    ResultadoFeedback res;

    pthread_mutex_lock(&trava);
    res.tipo = adaptador_classificar(alerta, houve);
    res.antes = atual;
    if (ADAPT_HABILITADO)
        atual = adaptador_passo(atual, r, h, alerta, houve, peso);
    res.depois = atual;
    switch (res.tipo) {
    case FB_TP: cont.tp++; break;
    case FB_FP: cont.fp++; break;
    case FB_FN: cont.fn++; break;
    default:    cont.tn++; break;
    }
    if (res.antes.chuva_mm != res.depois.chuva_mm ||
        res.antes.mare_m != res.depois.mare_m)
        cont.atualizacoes++;
    else if (res.tipo == FB_FN)
        cont.fn_ignorados++;
    salvar_estado();
    pthread_mutex_unlock(&trava);
    return res;
}
