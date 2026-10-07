#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "http_client.h"
#include "tempo_util.h"
#include "chuva.h"

int chuva_agregar(const RegistroChuva *registros, size_t n, DadosChuva *saida)
{
    long long ultima = -1;
    size_t i;

    if (saida == NULL)
        return 0;
    memset(saida, 0, sizeof(*saida));
    if (registros == NULL)
        return 0;

    for (i = 0; i < n; i++) {
        const RegistroChuva *r = &registros[i];
        long long chave;

        if (r->tem_chuva) {
            saida->soma_chuva += r->chuva;
            if (r->chuva > saida->maior_chuva)
                saida->maior_chuva = r->chuva;
            saida->total_estacoes++;
        }

        chave = tempo_chave_datahora(r->data_hora);
        if (chave >= ultima) {
            ultima = chave;
            strncpy(saida->ultima_leitura, r->data_hora,
                    sizeof(saida->ultima_leitura) - 1);
        }
    }

    saida->disponivel = saida->total_estacoes > 0;
    return saida->disponivel;
}

int chuva_obter(const char *url, DadosChuva *saida)
{
    char *raw;
    RegistroChuva *registros = NULL;
    int n, ok;

    if (saida == NULL)
        return 0;
    memset(saida, 0, sizeof(*saida));

    raw = http_get(url);
    if (raw == NULL)
        return 0;

    n = json_apac_extrair(raw, CIDADE_ALVO, &registros);
    free(raw);
    if (n <= 0) {
        free(registros);
        return 0;
    }

    ok = chuva_agregar(registros, (size_t)n, saida);
    free(registros);
    return ok;
}

double chuva_media(const DadosChuva *c)
{
    if (c == NULL || c->total_estacoes <= 0)
        return 0.0;
    return c->soma_chuva / c->total_estacoes;
}
