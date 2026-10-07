#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include "config.h"
#include "http_client.h"
#include "drenagem.h"
#include "sensor_virtual.h"
#include "relatorio.h"
#if MQTT_HABILITADO
#include "mqtt_publisher.h"
#endif

static volatile sig_atomic_t rodando = 1;

static void parar(int sinal)
{
    (void)sinal;
    rodando = 0;
}

int main(void)
{
    DadosDrenagem drenagem;
    LeituraVirtual leitura;

    signal(SIGINT, parar);
    signal(SIGTERM, parar);

    if (!http_init()) {
        fprintf(stderr, "Falha ao inicializar o cliente HTTP.\n");
        return 1;
    }

#if MQTT_HABILITADO
    if (!mqtt_iniciar())
        fprintf(stderr, "Aviso: MQTT indisponivel, seguindo sem publicar.\n");
#endif

    if (!drenagem_carregar(ARQUIVO_DRENAGEM, &drenagem))
        fprintf(stderr, "Aviso: drenagem nao carregada.\n");

    while (rodando) {
        sensor_ler(&drenagem, &leitura);
        relatorio_imprimir(&leitura);
#if MQTT_HABILITADO
        mqtt_publicar_leitura(&leitura);
#endif
        sleep(INTERVALO_LEITURA);
    }

#if MQTT_HABILITADO
    mqtt_encerrar();
#endif
    http_cleanup();
    return 0;
}
