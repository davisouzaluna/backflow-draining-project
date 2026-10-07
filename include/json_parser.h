#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include <stddef.h>

typedef struct {
    int tem_chuva;
    double chuva;
    char data_hora[32];
} RegistroChuva;

int json_apac_extrair(const char *raw_json, const char *cidade, RegistroChuva **saida);

#endif
