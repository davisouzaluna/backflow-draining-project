#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "tempo_util.h"
#include "mare_tabua.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
    time_t instante;
    char data[32];
    char hora[32];
    double altura;
} EventoMare;

static int comparar(const void *a, const void *b)
{
    time_t x = ((const EventoMare *)a)->instante;
    time_t y = ((const EventoMare *)b)->instante;

    return (x > y) - (x < y);
}

static int carregar_eventos(const char *arquivo, EventoMare **saida, size_t *n)
{
    FILE *f = fopen(arquivo, "r");
    EventoMare *v = NULL;
    size_t cap = 0;
    char linha[LINHA_MAX];

    *saida = NULL;
    *n = 0;
    if (f == NULL) {
        fprintf(stderr, "Nao foi possivel abrir a tabela de mare: %s\n", arquivo);
        return 0;
    }
    if (fgets(linha, sizeof(linha), f) == NULL) {
        fclose(f);
        return 0;
    }

    while (fgets(linha, sizeof(linha), f) != NULL) {
        EventoMare e;
        char *p;

        for (p = linha; *p; p++)
            if (*p == '\t')
                *p = ',';
        if (sscanf(linha, " %31[^,],%31[^,],%lf", e.data, e.hora, &e.altura) != 3)
            continue;
        if (!tempo_epoch(e.data, e.hora, &e.instante))
            continue;

        if (*n == cap) {
            size_t nova = cap ? cap * 2 : 256;
            EventoMare *tmp = realloc(v, nova * sizeof(*v));

            if (tmp == NULL) {
                free(v);
                fclose(f);
                return 0;
            }
            v = tmp;
            cap = nova;
        }
        v[(*n)++] = e;
    }
    fclose(f);
    if (*n > 1)
        qsort(v, *n, sizeof(*v), comparar);
    *saida = v;
    return *n > 0;
}

static void interpolar(const EventoMare *ev, size_t n, time_t agora, DadosMare *m)
{
    size_t i;

    for (i = 0; i + 1 < n; i++) {
        double span, frac;

        if (agora < ev[i].instante || agora >= ev[i + 1].instante)
            continue;
        span = difftime(ev[i + 1].instante, ev[i].instante);
        if (span <= 0.0 || span > MARE_GAP_MAX_S)
            return;
        frac = difftime(agora, ev[i].instante) / span;
        m->altura_interp = ev[i].altura +
            (ev[i + 1].altura - ev[i].altura) * (1.0 - cos(M_PI * frac)) / 2.0;
        m->interp_ok = 1;
        return;
    }
}

int mare_tabua_obter(const char *arquivo, time_t agora, DadosMare *mare)
{
    EventoMare *ev;
    size_t n, i, melhor = 0;
    double menor_dif = -1.0;

    if (arquivo == NULL || mare == NULL)
        return 0;
    memset(mare, 0, sizeof(*mare));
    if (!carregar_eventos(arquivo, &ev, &n))
        return 0;

    for (i = 0; i < n; i++) {
        double dif = fabs(difftime(ev[i].instante, agora));

        if (menor_dif < 0 || dif < menor_dif) {
            menor_dif = dif;
            melhor = i;
        }
    }

    strncpy(mare->data, ev[melhor].data, sizeof(mare->data) - 1);
    strncpy(mare->hora, ev[melhor].hora, sizeof(mare->hora) - 1);
    mare->altura = ev[melhor].altura;
    mare->menor_altura_dia = mare->altura;
    mare->maior_altura_dia = mare->altura;
    for (i = 0; i < n; i++) {
        if (strcmp(ev[i].data, mare->data) != 0)
            continue;
        if (ev[i].altura < mare->menor_altura_dia)
            mare->menor_altura_dia = ev[i].altura;
        if (ev[i].altura > mare->maior_altura_dia)
            mare->maior_altura_dia = ev[i].altura;
    }

    mare->diferenca_minutos = (long)(menor_dif / 60.0 + 0.5);
    interpolar(ev, n, agora, mare);
    mare->disponivel = mare->diferenca_minutos <= MARE_DIF_MAX_MIN;
    free(ev);
    return mare->disponivel;
}

double mare_posicao_relativa(const DadosMare *mare)
{
    double faixa;

    if (mare == NULL || !mare->disponivel)
        return -1.0;
    faixa = mare->maior_altura_dia - mare->menor_altura_dia;
    if (faixa <= 0.0)
        return -1.0;
    return (mare->altura - mare->menor_altura_dia) / faixa;
}

double mare_altura_efetiva(const DadosMare *mare)
{
    if (mare == NULL || !mare->disponivel)
        return 0.0;
    return mare->interp_ok ? mare->altura_interp : mare->altura;
}
