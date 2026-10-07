#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "tempo_util.h"
#include "mare_tabua.h"

typedef struct {
    time_t instante;
    char data[32];
    char hora[32];
    double altura;
} EventoMare;

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
    *saida = v;
    return *n > 0;
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
        double dif = difftime(ev[i].instante, agora);
        if (dif < 0)
            dif = -dif;
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
    mare->disponivel = 1;
    free(ev);
    return 1;
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
