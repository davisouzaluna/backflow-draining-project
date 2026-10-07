#ifndef DRENAGEM_H
#define DRENAGEM_H

typedef struct {
    int bocas_lobo;
    int pocos_visita;
    int caixas_gaveta;
    int total_elementos;
} DadosDrenagem;

int drenagem_carregar(const char *arquivo, DadosDrenagem *saida);

#endif
