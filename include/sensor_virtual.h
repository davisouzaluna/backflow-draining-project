#ifndef SENSOR_VIRTUAL_H
#define SENSOR_VIRTUAL_H

#include <time.h>
#include "chuva.h"
#include "mare_tabua.h"
#include "estimacao_mare.h"
#include "drenagem.h"
#include "analisador.h"

typedef struct {
    long long boot_id;
    unsigned long seq;
    time_t epoch;
    char timestamp[64];
    DadosChuva chuva;
    double chuva_media;
    DadosMare mare;
    ResultadoMare mare_harmonica;
    DadosDrenagem drenagem;
    Analise analise;
    int pergunta;
    char pergunta_motivo[16];
} LeituraVirtual;

void sensor_ler(const DadosDrenagem *drenagem, LeituraVirtual *leitura);

#endif
