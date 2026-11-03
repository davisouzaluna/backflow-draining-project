#include <stdio.h>
#include <time.h>
#include "config.h"
#include "log_bruto.h"

int log_bruto_salvar(const char *payload)
{
#if LOG_BRUTO_HABILITADO
    static int contador;
    char caminho[256], ts[32];
    time_t agora;
    struct tm *t;
    FILE *f;

    if (payload == NULL || contador++ % LOG_BRUTO_A_CADA != 0)
        return 0;

    agora = time(NULL);
    t = localtime(&agora);
    if (t == NULL)
        return 0;

    strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", t);
    snprintf(caminho, sizeof(caminho), DIR_BRUTO "/apac_%s.json", ts);

    f = fopen(caminho, "w");
    if (f == NULL)
        return 0;
    fputs(payload, f);
    fclose(f);
    return 1;
#else
    (void)payload;
    return 0;
#endif
}
