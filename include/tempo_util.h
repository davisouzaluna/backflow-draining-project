#ifndef TEMPO_UTIL_H
#define TEMPO_UTIL_H

#include <stddef.h>
#include <time.h>

long long tempo_chave(int ano, int mes, int dia, int hora, int minuto);
long long tempo_chave_datahora(const char *data_hora);
long long tempo_chave_data_hora(const char *data, const char *hora);
int tempo_epoch(const char *data, const char *hora, time_t *saida);
void tempo_agora_str(char *buffer, size_t tamanho);

#endif
