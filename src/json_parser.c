#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>
#include "config.h"
#include "json_parser.h"

static int reservar(RegistroChuva **v, size_t *cap, size_t n)
{
    RegistroChuva *novo;
    size_t nova_cap;

    if (n < *cap)
        return 1;
    nova_cap = (*cap == 0) ? 32 : *cap * 2;
    novo = realloc(*v, nova_cap * sizeof(**v));
    if (novo == NULL)
        return 0;
    *v = novo;
    *cap = nova_cap;
    return 1;
}

static void preencher(RegistroChuva *r, const cJSON *obj)
{
    const cJSON *chuva = cJSON_GetObjectItemCaseSensitive(obj, JSON_CAMPO_CHUVA);
    const cJSON *dh = cJSON_GetObjectItemCaseSensitive(obj, JSON_CAMPO_DATAHORA);

    memset(r, 0, sizeof(*r));
    if (cJSON_IsNumber(chuva)) {
        r->tem_chuva = 1;
        r->chuva = chuva->valuedouble;
    }
    if (cJSON_IsString(dh))
        strncpy(r->data_hora, dh->valuestring, sizeof(r->data_hora) - 1);
}

int json_apac_extrair(const char *raw_json, const char *cidade, RegistroChuva **saida)
{
    cJSON *root;
    const cJSON *item;
    RegistroChuva *v = NULL;
    size_t n = 0, cap = 0;

    if (saida == NULL)
        return -1;
    *saida = NULL;
    if (raw_json == NULL || cidade == NULL)
        return -1;

    root = cJSON_Parse(raw_json);
    if (root == NULL) {
        fprintf(stderr, "Erro ao interpretar JSON da APAC.\n");
        return -1;
    }
    if (!cJSON_IsArray(root)) {
        fprintf(stderr, "Erro: resposta da APAC nao e um array JSON.\n");
        cJSON_Delete(root);
        return -1;
    }

    cJSON_ArrayForEach(item, root) {
        const cJSON *env = cJSON_GetObjectItemCaseSensitive(item, JSON_CAMPO_ENVELOPE);
        const cJSON *cid;
        cJSON *obj;

        if (!cJSON_IsString(env))
            continue;
        obj = cJSON_Parse(env->valuestring);
        if (obj == NULL)
            continue;

        cid = cJSON_GetObjectItemCaseSensitive(obj, JSON_CAMPO_CIDADE);
        if (cJSON_IsString(cid) && strcmp(cid->valuestring, cidade) == 0) {
            if (!reservar(&v, &cap, n)) {
                cJSON_Delete(obj);
                cJSON_Delete(root);
                free(v);
                return -1;
            }
            preencher(&v[n++], obj);
        }
        cJSON_Delete(obj);
    }

    cJSON_Delete(root);
    *saida = v;
    return (int)n;
}
