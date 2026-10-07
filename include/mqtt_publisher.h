#ifndef MQTT_PUBLISHER_H
#define MQTT_PUBLISHER_H

#include "sensor_virtual.h"

int mqtt_iniciar(void);
int mqtt_publicar_leitura(const LeituraVirtual *l);
void mqtt_encerrar(void);

#endif
