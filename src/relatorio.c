#include <stdio.h>
#include "config.h"
#include "relatorio.h"

static void linha_meta(const char *nome, const Avaliacao *a)
{
    printf("  %-20s valor=%.2f meta=%.2f -> %s\n", nome, a->valor, a->meta,
           analisador_nome_comparacao(a->resultado));
}

void relatorio_imprimir(const LeituraVirtual *l)
{
    const Analise *a = &l->analise;

    printf("\n=== SENSOR VIRTUAL RECIFE | %s ===\n", l->timestamp);

    printf("Chuva: media=%.2f mm | maior=%.2f mm | estacoes=%d | ultima=%s\n",
           l->chuva_media, l->chuva.maior_chuva, l->chuva.total_estacoes,
           l->chuva.ultima_leitura);

    if (l->mare.disponivel)
        printf("Mare (tabua): %s %s | %.2f m | dia: %.2f-%.2f m | dif=%ld min\n",
               l->mare.data, l->mare.hora, l->mare.altura,
               l->mare.menor_altura_dia, l->mare.maior_altura_dia,
               l->mare.diferenca_minutos);
    else
        printf("Mare (tabua): indisponivel\n");

#if HABILITAR_MARE_HARMONICA
    printf("Mare (harmonica): %.2f m | %.2f m/h | %s\n",
           l->mare_harmonica.altura_m, l->mare_harmonica.velocidade_m_h,
           l->mare_harmonica.estado);
#endif

    printf("Drenagem: BL=%d PV=%d CG=%d total=%d\n", l->drenagem.bocas_lobo,
           l->drenagem.pocos_visita, l->drenagem.caixas_gaveta,
           l->drenagem.total_elementos);

    linha_meta("chuva atencao", &a->chuva_atencao);
    linha_meta("chuva critica", &a->chuva_critica);
    linha_meta("mare intermediaria", &a->mare_intermediaria);
    linha_meta("mare alta", &a->mare_alta);

    printf("RESULTADO: %s\n", analisador_nome_risco(a->risco));
}
