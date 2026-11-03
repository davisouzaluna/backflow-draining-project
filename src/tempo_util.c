#include <stdio.h>
#include "tempo_util.h"

long long tempo_chave(int ano, int mes, int dia, int hora, int minuto)
{
    return (long long)ano * 100000000LL + (long long)mes * 1000000LL +
           (long long)dia * 10000LL + (long long)hora * 100LL + minuto;
}

long long tempo_chave_datahora(const char *data_hora)
{
    int a, m, d, h, mi;

    if (data_hora == NULL ||
        sscanf(data_hora, "%d-%d-%d %d:%d", &a, &m, &d, &h, &mi) != 5)
        return -1;
    return tempo_chave(a, m, d, h, mi);
}

long long tempo_chave_data_hora(const char *data, const char *hora)
{
    int a, m, d, h, mi;

    if (data == NULL || hora == NULL ||
        sscanf(data, "%d-%d-%d", &a, &m, &d) != 3 ||
        sscanf(hora, "%d:%d", &h, &mi) != 2)
        return -1;
    return tempo_chave(a, m, d, h, mi);
}

int tempo_epoch(const char *data, const char *hora, time_t *saida)
{
    int a, m, d, h, mi;
    struct tm t = {0};
    time_t r;

    if (data == NULL || hora == NULL || saida == NULL ||
        sscanf(data, "%d-%d-%d", &a, &m, &d) != 3 ||
        sscanf(hora, "%d:%d", &h, &mi) != 2)
        return 0;

    t.tm_year = a - 1900;
    t.tm_mon = m - 1;
    t.tm_mday = d;
    t.tm_hour = h;
    t.tm_min = mi;
    t.tm_isdst = -1;

    r = mktime(&t);
    if (r == (time_t)-1)
        return 0;
    *saida = r;
    return 1;
}

void tempo_agora_str(char *buffer, size_t tamanho)
{
    time_t agora = time(NULL);
    struct tm *t = localtime(&agora);

    if (t == NULL) {
        snprintf(buffer, tamanho, "timestamp-indisponivel");
        return;
    }
    strftime(buffer, tamanho, "%Y-%m-%d %H:%M:%S", t);
}

static long long dias_desde_epoch(int a, int m, int d)
{
    long long y = a - (m <= 2);
    long long era = (y >= 0 ? y : y - 399) / 400;
    long long yoe = y - era * 400;
    long long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

int tempo_epoch_datahora(const char *data_hora, int utc, time_t *saida)
{
    int a, m, d, h, mi;

    if (data_hora == NULL || saida == NULL ||
        sscanf(data_hora, "%d-%d-%d %d:%d", &a, &m, &d, &h, &mi) != 5)
        return 0;

    if (utc) {
        *saida = (time_t)(dias_desde_epoch(a, m, d) * 86400LL + h * 3600LL + mi * 60LL);
        return 1;
    }

    {
        struct tm t = {0};
        time_t r;

        t.tm_year = a - 1900;
        t.tm_mon = m - 1;
        t.tm_mday = d;
        t.tm_hour = h;
        t.tm_min = mi;
        t.tm_isdst = -1;
        r = mktime(&t);
        if (r == (time_t)-1)
            return 0;
        *saida = r;
        return 1;
    }
}
