#include <stdio.h>
#include <string.h>
#include "config.h"
#include "drenagem.h"

int drenagem_carregar(const char *arquivo, DadosDrenagem *saida)
{
    FILE *f;
    char linha[LINHA_MAX];

    if (arquivo == NULL || saida == NULL)
        return 0;
    memset(saida, 0, sizeof(*saida));

    f = fopen(arquivo, "r");
    if (f == NULL) {
        fprintf(stderr, "Nao foi possivel abrir o arquivo de drenagem: %s\n", arquivo);
        return 0;
    }

    if (fgets(linha, sizeof(linha), f) == NULL) {
        fclose(f);
        return 0;
    }

    while (fgets(linha, sizeof(linha), f) != NULL) {
        char id[32], tipo[32], lat[64], lon[64];

        if (sscanf(linha, " %31[^,],%31[^,],%63[^,],%63[^\n\r]", id, tipo, lat, lon) != 4)
            continue;

        if (strcmp(tipo, DRENAGEM_TIPO_BL) == 0)
            saida->bocas_lobo++;
        else if (strcmp(tipo, DRENAGEM_TIPO_PV) == 0)
            saida->pocos_visita++;
        else if (strcmp(tipo, DRENAGEM_TIPO_CG) == 0)
            saida->caixas_gaveta++;
        else
            continue;

        saida->total_elementos++;
    }

    fclose(f);
    return saida->total_elementos > 0;
}
