#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "config.h"
#include "http_client.h"
#include "drenagem.h"
#include "sensor_virtual.h"
#include "relatorio.h"
#include "log_csv.h"
#include "adaptador.h"
#include "feedback.h"
#if MQTT_HABILITADO
#include "mqtt_publisher.h"
#endif

static volatile sig_atomic_t rodando = 1;

static void parar(int sinal)
{
    (void)sinal;
    rodando = 0;
}

int main(int argc, char **argv)
{
    DadosDrenagem drenagem;
    LeituraVirtual leitura;
    int uma_vez = argc > 1 && strcmp(argv[1], "--once") == 0;
    int usar_mqtt = MQTT_HABILITADO && !uma_vez;

    signal(SIGINT, parar);
    signal(SIGTERM, parar);

    if (!http_init()) {
        fprintf(stderr, "Falha ao inicializar o cliente HTTP.\n");
        return 1;
    }

#if MQTT_HABILITADO
    if (usar_mqtt && !mqtt_iniciar()) {
        fprintf(stderr, "Aviso: MQTT indisponivel, seguindo sem publicar.\n");
        usar_mqtt = 0;
    }
#endif

    adaptador_iniciar();

    if (!drenagem_carregar(ARQUIVO_DRENAGEM, &drenagem))
        fprintf(stderr, "Aviso: drenagem nao carregada.\n");

    while (rodando) {
        sensor_ler(&drenagem, &leitura);
        feedback_registrar_leitura(&leitura);
        relatorio_imprimir(&leitura);
#if LOG_HABILITADO
        log_csv_registrar(&leitura);
#endif
#if MQTT_HABILITADO
        if (usar_mqtt)
            mqtt_publicar_leitura(&leitura);
#endif
        if (uma_vez)
            break;
        sleep(INTERVALO_LEITURA);
    }

#if MQTT_HABILITADO
    if (usar_mqtt)
        mqtt_encerrar();
#endif
    http_cleanup();
    return 0;
}
