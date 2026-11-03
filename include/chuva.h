#ifndef CHUVA_H
#define CHUVA_H

#include <stddef.h>
#include "json_parser.h"

typedef struct {
    int disponivel;
    double soma_chuva;
    double maior_chuva;
    int total_estacoes;
    char ultima_leitura[64];
    int curl_rc;
    long http_code;
    double fetch_ms;
    double connect_ms;
    double ttfb_ms;
    size_t bytes;
    double idade_dado_s;
} DadosChuva;

int chuva_agregar(const RegistroChuva *registros, size_t n, DadosChuva *saida);
int chuva_obter(const char *url, DadosChuva *saida);
double chuva_media(const DadosChuva *c);

#endif
