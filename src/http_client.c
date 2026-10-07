#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "config.h"
#include "http_client.h"

typedef struct {
    char *dados;
    size_t tamanho;
} Buffer;

static size_t write_cb(void *conteudo, size_t size, size_t nmemb, void *userp)
{
    Buffer *b = (Buffer *)userp;
    size_t total = size * nmemb;
    char *novo = realloc(b->dados, b->tamanho + total + 1);

    if (novo == NULL) {
        fprintf(stderr, "Erro: memoria insuficiente.\n");
        return 0;
    }
    b->dados = novo;
    memcpy(b->dados + b->tamanho, conteudo, total);
    b->tamanho += total;
    b->dados[b->tamanho] = '\0';
    return total;
}

int http_init(void)
{
    return curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
}

void http_cleanup(void)
{
    curl_global_cleanup();
}

char *http_get(const char *url)
{
    CURL *curl;
    CURLcode rc;
    Buffer b = {NULL, 0};

    curl = curl_easy_init();
    if (curl == NULL) {
        fprintf(stderr, "Erro ao inicializar CURL.\n");
        return NULL;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, HTTP_CONNECT_TIMEOUT_S);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, HTTP_TIMEOUT_S);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &b);

    rc = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (rc != CURLE_OK) {
        fprintf(stderr, "Erro na requisicao HTTP: %s\n", curl_easy_strerror(rc));
        free(b.dados);
        return NULL;
    }
    return b.dados;
}
