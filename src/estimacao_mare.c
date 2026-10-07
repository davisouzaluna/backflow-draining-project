#ifndef ESTIMACAO_MARE_H
#define ESTIMACAO_MARE_H

#include <stdio.h>
#include <math.h>
#include <time.h>
#include <stdbool.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Estrutura de resultado do Sensor Virtual de Maré */
typedef struct {
    double altura_m;            /* Altura h(t) em metros */
    double velocidade_m_h;      /* dh/dt em m/h (+ enchente, - vazante) */
    bool enchente;              /* true se a maré estiver subindo */
    char estado[32];            /* Texto: "ENCHENTE", "VAZANTE", "PREAMAR", "BAIXA-MAR" */
} ResultadoMare;

/* Componente harmônica no padrão CHM */
typedef struct {
    const char *nome;
    double A;        /* Amplitude (m) */
    double omega;    /* Velocidade angular (graus/hora) */
    double V0_u;     /* (V0 + u) em graus */
    double g;        /* Retardamento de fase local em graus */
    double f;        /* Fator nodal */
} ComponenteHarmonica;

/* Constantes para o Porto do Recife (Z0 ~ 1.35m no Zero Hidrográfico) */
#define Z0_RECIFE 1.35

static const ComponenteHarmonica TABELA_RECIFE[] = {
    {"M2", 0.85, 28.9841042, 0.0, 115.0, 1.00},
    {"S2", 0.38, 30.0000000, 0.0, 130.0, 1.00},
    {"N2", 0.18, 28.4397295, 0.0, 110.0, 1.00},
    {"K1", 0.10, 15.0410686, 0.0,  85.0, 1.00},
    {"O1", 0.08, 13.9430356, 0.0,  70.0, 1.00},
    {"M4", 0.03, 57.9682084, 0.0, 230.0, 1.00}  /* Componente de águas rasas */
};

#define QTD_COMPONENTES (sizeof(TABELA_RECIFE) / sizeof(ComponenteHarmonica))

static inline double deg2rad(double deg) {
    return deg * (M_PI / 180.0);
}

/*
 * Calcula a maré astronômica e sua tendência diretamente a partir de um timestamp UNIX
 */
ResultadoMare calcular_sensor_mare(time_t timestamp_atual) {
    ResultadoMare res = {0};
    
    /* 1. Descobre o início do ano corrente (01/Jan 00:00:00) */
    struct tm *tm_info = localtime(&timestamp_atual);
    struct tm tm_jan1 = *tm_info;
    tm_jan1.tm_mon = 0;
    tm_jan1.tm_mday = 1;
    tm_jan1.tm_hour = 0;
    tm_jan1.tm_min = 0;
    tm_jan1.tm_sec = 0;
    
    time_t t_jan1 = mktime(&tm_jan1);
    
    /* 2. Tempo t decorrido em horas desde 1º de Janeiro */
    double t_horas = difftime(timestamp_atual, t_jan1) / 3600.0;

    double h_t = Z0_RECIFE;
    double dh_dt = 0.0;

    /* 3. Somatório Harmônico e Derivada Temporal */
    for (size_t i = 0; i < QTD_COMPONENTES; i++) {
        double omega_rad_h = deg2rad(TABELA_RECIFE[i].omega);
        
        /* Fase angular = omega*t + (V0+u) - g */
        double fase_deg = (TABELA_RECIFE[i].omega * t_horas) 
                        + TABELA_RECIFE[i].V0_u 
                        - TABELA_RECIFE[i].g;
        
        /* Normaliza o ângulo para a faixa [0, 360) para manter precisão de float */
        fase_deg = fmod(fase_deg, 360.0);
        if (fase_deg < 0) fase_deg += 360.0;
        
        double fase_rad = deg2rad(fase_deg);

        /* Altura: Z0 + sum(f * A * cos(fase)) */
        h_t += TABELA_RECIFE[i].f * TABELA_RECIFE[i].A * cos(fase_rad);

        /* Velocidade (dh/dt): - sum(f * A * omega * sin(fase)) */
        dh_dt -= TABELA_RECIFE[i].f * TABELA_RECIFE[i].A * omega_rad_h * sin(fase_rad);
    }

    res.altura_m = h_t;
    res.velocidade_m_h = dh_dt;
    res.enchente = (dh_dt > 0.0);

    /* Classificação do estado dinâmico */
    if (fabs(dh_dt) < 0.05) {
        snprintf(res.estado, sizeof(res.estado), (h_t > Z0_RECIFE) ? "PREAMAR (PICO)" : "BAIXA-MAR (FUNDO)");
    } else if (dh_dt > 0) {
        snprintf(res.estado, sizeof(res.estado), "ENCHENTE (SUBINDO)");
    } else {
        snprintf(res.estado, sizeof(res.estado), "VAZANTE (DESCENDO)");
    }

    return res;
}

#endif
