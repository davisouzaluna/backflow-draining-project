#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mosquitto.h>
#include "config.h"
#include "payload.h"
#include "mqtt_publisher.h"

static struct mosquitto *cliente;

static void publicar_status(const char *texto)
{
    mosquitto_publish(cliente, NULL, MQTT_TOPICO_STATUS, (int)strlen(texto),
                      texto, MQTT_QOS, true);
}

static void on_connect(struct mosquitto *m, void *obj, int rc)
{
    (void)m;
    (void)obj;
    if (rc == 0)
        publicar_status("online");
    else
        fprintf(stderr, "MQTT: conexao recusada (rc=%d)\n", rc);
}

int mqtt_iniciar(void)
{
    mosquitto_lib_init();

    cliente = mosquitto_new(MQTT_CLIENT_ID, true, NULL);
    if (cliente == NULL) {
        fprintf(stderr, "MQTT: falha ao criar cliente.\n");
        return 0;
    }

    if (MQTT_USUARIO[0] != '\0')
        mosquitto_username_pw_set(cliente, MQTT_USUARIO, MQTT_SENHA);

    mosquitto_will_set(cliente, MQTT_TOPICO_STATUS, 7, "offline", MQTT_QOS, true);
    mosquitto_connect_callback_set(cliente, on_connect);
    mosquitto_reconnect_delay_set(cliente, 2, 30, true);

    if (mosquitto_connect_async(cliente, MQTT_HOST, MQTT_PORTA, MQTT_KEEPALIVE_S)
            != MOSQ_ERR_SUCCESS ||
        mosquitto_loop_start(cliente) != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "MQTT: falha ao iniciar conexao com %s:%d\n",
                MQTT_HOST, MQTT_PORTA);
        return 0;
    }
    return 1;
}

int mqtt_publicar_leitura(const LeituraVirtual *l)
{
    char *json;
    int rc;

    if (cliente == NULL)
        return 0;

    json = payload_serializar(l);
    if (json == NULL)
        return 0;

    rc = mosquitto_publish(cliente, NULL, MQTT_TOPICO_LEITURA, (int)strlen(json),
                           json, MQTT_QOS, true);
    free(json);

    if (rc != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "MQTT: publicacao falhou (%s)\n", mosquitto_strerror(rc));
        return 0;
    }
    return 1;
}

void mqtt_encerrar(void)
{
    if (cliente == NULL)
        return;
    publicar_status("offline");
    mosquitto_disconnect(cliente);
    mosquitto_loop_stop(cliente, false);
    mosquitto_destroy(cliente);
    cliente = NULL;
    mosquitto_lib_cleanup();
}
