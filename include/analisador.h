#ifndef ANALISADOR_H
#define ANALISADOR_H

#include "chuva.h"
#include "limiares.h"
#include "mare_tabua.h"

typedef enum {
    META_SEM_DADO,
    META_ABAIXO,
    META_ACIMA
} ComparacaoMeta;

typedef enum {
    RISCO_INDETERMINADO,
    RISCO_NENHUM,
    RISCO_POTENCIAL,
    RISCO_OCORRENDO
} NivelRisco;

typedef struct {
    double valor;
    double meta;
    ComparacaoMeta resultado;
} Avaliacao;

typedef struct {
    Avaliacao chuva_margem;
    Avaliacao chuva_limiar;
    Avaliacao mare_margem;
    Avaliacao mare_limiar;
    NivelRisco risco;
    int alerta;
    double mare_m;
    Limiares limiares;
} Analise;

Avaliacao analisador_avaliar(double valor, double meta, int disponivel);
Analise analisador_executar(const DadosChuva *chuva, const DadosMare *mare,
                            Limiares lim);
const char *analisador_nome_comparacao(ComparacaoMeta c);
const char *analisador_nome_risco(NivelRisco r);

#endif
