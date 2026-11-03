#include <stdio.h>
#include "config.h"
#include "relatorio.h"

static void linha_meta(const char *nome, const Avaliacao *a, const char *un)
{
    printf("  %-16s valor=%.2f %s meta=%.2f %s -> %s\n", nome, a->valor, un,
           a->meta, un, analisador_nome_comparacao(a->resultado));
}

void relatorio_imprimir(const LeituraVirtual *l)
{
    const Analise *a = &l->analise;

    printf("\n=== SENSOR VIRTUAL RECIFE | %s ===\n", l->timestamp);

    printf("Chuva: media=%.2f mm | maior=%.2f mm | estacoes=%d | ultima=%s\n",
           l->chuva_media, l->chuva.maior_chuva, l->chuva.total_estacoes,
           l->chuva.ultima_leitura);

    if (l->mare.disponivel)
        printf("Mare (tabua): evento %s %s %.2f m | agora (interp.) %.2f m | dif=%ld min\n",
               l->mare.data, l->mare.hora, l->mare.altura,
               mare_altura_efetiva(&l->mare), l->mare.diferenca_minutos);
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

    printf("Limiares vigentes: chuva=%.2f mm | mare=%.2f m\n",
           a->limiares.chuva_mm, a->limiares.mare_m);
    linha_meta("chuva (margem)", &a->chuva_margem, "mm");
    linha_meta("chuva (limiar)", &a->chuva_limiar, "mm");
    linha_meta("mare (margem)", &a->mare_margem, "m");
    linha_meta("mare (limiar)", &a->mare_limiar, "m");

    printf("RESULTADO: %s\n", analisador_nome_risco(a->risco));
}
