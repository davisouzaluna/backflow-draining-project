#ifndef ESTIMACAO_MARE_H
#define ESTIMACAO_MARE_H

#include <time.h>
#include <stdbool.h>

/* Estrutura de resultado do Sensor Virtual de Maré */
typedef struct {
    double altura_m;            /* Altura h(t) em metros */
    double velocidade_m_h;      /* dh/dt em m/h (+ enchente, - vazante) */
    bool enchente;              /* true se a maré estiver subindo */
    char estado[32];            /* Texto: "ENCHENTE", "VAZANTE", "PREAMAR", "BAIXA-MAR" */
} ResultadoMare;

/* Protótipo da função principal de cálculo */
ResultadoMare calcular_sensor_mare(time_t timestamp_atual);

#endif /* ESTIMACAO_MARE_H */