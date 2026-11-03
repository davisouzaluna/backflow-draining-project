#ifndef FEEDBACK_H
#define FEEDBACK_H

#include <stddef.h>
#include "sensor_virtual.h"

typedef struct {
    unsigned long recebidos;
    unsigned long aceitos;
    unsigned long rejeitados;
} EstatisticaFeedback;

void feedback_registrar_leitura(LeituraVirtual *l);
int feedback_processar(const char *json, size_t n);
EstatisticaFeedback feedback_estatistica(void);

#endif
