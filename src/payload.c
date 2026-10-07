#include <cjson/cJSON.h>
#include "analisador.h"
#include "mare_tabua.h"
#include "payload.h"

static void add_meta(cJSON *pai, const char *nome, const Avaliacao *a)
{
    cJSON *o = cJSON_AddObjectToObject(pai, nome);

    cJSON_AddNumberToObject(o, "valor", a->valor);
    cJSON_AddNumberToObject(o, "meta", a->meta);
    cJSON_AddStringToObject(o, "resultado", analisador_nome_comparacao(a->resultado));
}

char *payload_serializar(const LeituraVirtual *l)
{
    cJSON *raiz = cJSON_CreateObject();
    cJSON *o;
    char *texto;

    if (raiz == NULL)
        return NULL;

    cJSON_AddStringToObject(raiz, "timestamp", l->timestamp);

    o = cJSON_AddObjectToObject(raiz, "chuva");
    cJSON_AddBoolToObject(o, "disponivel", l->chuva.disponivel);
    cJSON_AddNumberToObject(o, "media_mm", l->chuva_media);
    cJSON_AddNumberToObject(o, "maior_mm", l->chuva.maior_chuva);
    cJSON_AddNumberToObject(o, "estacoes", l->chuva.total_estacoes);
    cJSON_AddStringToObject(o, "ultima_leitura", l->chuva.ultima_leitura);

    o = cJSON_AddObjectToObject(raiz, "mare");
    cJSON_AddBoolToObject(o, "disponivel", l->mare.disponivel);
    cJSON_AddStringToObject(o, "evento_data", l->mare.data);
    cJSON_AddStringToObject(o, "evento_hora", l->mare.hora);
    cJSON_AddNumberToObject(o, "altura_m", l->mare.altura);
    cJSON_AddNumberToObject(o, "min_dia_m", l->mare.menor_altura_dia);
    cJSON_AddNumberToObject(o, "max_dia_m", l->mare.maior_altura_dia);
    cJSON_AddNumberToObject(o, "posicao", mare_posicao_relativa(&l->mare));
    cJSON_AddNumberToObject(o, "dif_min", (double)l->mare.diferenca_minutos);

    o = cJSON_AddObjectToObject(raiz, "drenagem");
    cJSON_AddNumberToObject(o, "bocas_lobo", l->drenagem.bocas_lobo);
    cJSON_AddNumberToObject(o, "pocos_visita", l->drenagem.pocos_visita);
    cJSON_AddNumberToObject(o, "caixas_gaveta", l->drenagem.caixas_gaveta);
    cJSON_AddNumberToObject(o, "total", l->drenagem.total_elementos);

    o = cJSON_AddObjectToObject(raiz, "metas");
    add_meta(o, "chuva_atencao", &l->analise.chuva_atencao);
    add_meta(o, "chuva_critica", &l->analise.chuva_critica);
    add_meta(o, "mare_intermediaria", &l->analise.mare_intermediaria);
    add_meta(o, "mare_alta", &l->analise.mare_alta);

    o = cJSON_AddObjectToObject(raiz, "risco");
    cJSON_AddNumberToObject(o, "codigo", l->analise.risco);
    cJSON_AddStringToObject(o, "nome", analisador_nome_risco(l->analise.risco));

    texto = cJSON_PrintUnformatted(raiz);
    cJSON_Delete(raiz);
    return texto;
}
