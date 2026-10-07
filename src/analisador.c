#include "config.h"
#include "analisador.h"

static int acima(const Avaliacao *a)
{
    return a->resultado == META_ACIMA;
}

Avaliacao analisador_avaliar(double valor, double meta, int disponivel)
{
    Avaliacao a;

    a.valor = valor;
    a.meta = meta;
    if (!disponivel)
        a.resultado = META_SEM_DADO;
    else
        a.resultado = (valor >= meta) ? META_ACIMA : META_ABAIXO;
    return a;
}

static NivelRisco classificar_risco(const Analise *a)
{
    if (a->chuva_critica.resultado == META_SEM_DADO ||
        a->mare_alta.resultado == META_SEM_DADO)
        return RISCO_INDETERMINADO;

    if (acima(&a->chuva_critica) && acima(&a->mare_alta))
        return RISCO_OCORRENDO;

    if (acima(&a->chuva_critica) || acima(&a->mare_alta) ||
        (acima(&a->chuva_atencao) && acima(&a->mare_intermediaria)))
        return RISCO_POTENCIAL;

    return RISCO_NENHUM;
}

Analise analisador_executar(const DadosChuva *chuva, const DadosMare *mare)
{
    Analise a;
    int ok_chuva = chuva != NULL && chuva->disponivel;
    double media = ok_chuva ? chuva_media(chuva) : 0.0;
    double pos = mare_posicao_relativa(mare);
    int ok_mare = pos >= 0.0;

    a.chuva_atencao = analisador_avaliar(media, META_CHUVA_ATENCAO_MM, ok_chuva);
    a.chuva_critica = analisador_avaliar(media, META_CHUVA_CRITICA_MM, ok_chuva);
    a.mare_intermediaria = analisador_avaliar(pos, META_MARE_INTERMED_POS, ok_mare);
    a.mare_alta = analisador_avaliar(pos, META_MARE_ALTA_POS, ok_mare);
    a.risco = classificar_risco(&a);
    return a;
}

const char *analisador_nome_comparacao(ComparacaoMeta c)
{
    switch (c) {
    case META_ACIMA:  return "ACIMA";
    case META_ABAIXO: return "ABAIXO";
    default:          return "SEM DADO";
    }
}

const char *analisador_nome_risco(NivelRisco r)
{
    switch (r) {
    case RISCO_OCORRENDO: return "BACKFLOW EM OCORRENCIA";
    case RISCO_POTENCIAL: return "RISCO DE BACKFLOW";
    case RISCO_NENHUM:    return "SEM RISCO";
    default:              return "DADOS INSUFICIENTES";
    }
}
