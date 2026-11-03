#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "config.h"
#include "http_client.h"
#include "tempo_util.h"
#include "log_bruto.h"
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
    HttpMetricas hm;
    int n, ok = 0;
    time_t fonte;

    if (saida == NULL)
        return 0;
    memset(saida, 0, sizeof(*saida));

    raw = http_get_ex(url, &hm);
    if (raw != NULL) {
        log_bruto_salvar(raw);
        n = json_apac_extrair(raw, CIDADE_ALVO, &registros);
        free(raw);
        if (n > 0)
            ok = chuva_agregar(registros, (size_t)n, saida);
        free(registros);
    }

    saida->curl_rc = hm.curl_rc;
    saida->http_code = hm.http_code;
    saida->fetch_ms = hm.total_ms;
    saida->connect_ms = hm.connect_ms;
    saida->ttfb_ms = hm.ttfb_ms;
    saida->bytes = hm.bytes;
    saida->idade_dado_s = -1.0;
    if (saida->ultima_leitura[0] != '\0' &&
        tempo_epoch_datahora(saida->ultima_leitura, APAC_DATAHORA_UTC, &fonte))
        saida->idade_dado_s = difftime(time(NULL), fonte);
    return ok;
}

double chuva_media(const DadosChuva *c)
{
    if (c == NULL || c->total_estacoes <= 0)
        return 0.0;
    return c->soma_chuva / c->total_estacoes;
}
