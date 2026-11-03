#ifndef MARE_TABUA_H
#define MARE_TABUA_H

#include <time.h>

typedef struct {
    int disponivel;
    int interp_ok;
    char data[32];
    char hora[32];
    double altura;
    double altura_interp;
    double menor_altura_dia;
    double maior_altura_dia;
    long diferenca_minutos;
} DadosMare;

int mare_tabua_obter(const char *arquivo, time_t agora, DadosMare *mare);
double mare_posicao_relativa(const DadosMare *mare);
double mare_altura_efetiva(const DadosMare *mare);

#endif
