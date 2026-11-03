#include <string.h>
#include <time.h>
#include "config.h"
#include "tempo_util.h"
#include "adaptador.h"
#include "sensor_virtual.h"

void sensor_ler(const DadosDrenagem *drenagem, LeituraVirtual *l)
{
    static unsigned long contador;
    static time_t boot_id;
    time_t agora = time(NULL);

    if (boot_id == 0)
        boot_id = agora;
    memset(l, 0, sizeof(*l));
    l->epoch = agora;
    l->boot_id = (long long)boot_id;
    l->seq = ++contador;
    tempo_agora_str(l->timestamp, sizeof(l->timestamp));

    if (drenagem != NULL)
        l->drenagem = *drenagem;

    chuva_obter(APAC_URL, &l->chuva);
    l->chuva_media = chuva_media(&l->chuva);
    mare_tabua_obter(ARQUIVO_MARE, agora, &l->mare);

#if HABILITAR_MARE_HARMONICA
    l->mare_harmonica = calcular_sensor_mare(agora);
#endif

    l->analise = analisador_executar(&l->chuva, &l->mare, adaptador_limiares());
}
