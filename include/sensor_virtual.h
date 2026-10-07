#ifndef SENSOR_VIRTUAL_H
#define SENSOR_VIRTUAL_H

#include "chuva.h"
#include "mare_tabua.h"
#include "estimacao_mare.h"
#include "drenagem.h"
#include "analisador.h"

typedef struct {
    char timestamp[64];
    DadosChuva chuva;
    double chuva_media;
    DadosMare mare;
    ResultadoMare mare_harmonica;
    DadosDrenagem drenagem;
    Analise analise;
} LeituraVirtual;

void sensor_ler(const DadosDrenagem *drenagem, LeituraVirtual *leitura);

#endif
