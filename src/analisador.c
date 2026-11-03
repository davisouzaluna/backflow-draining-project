#include <string.h>
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
    if (a->chuva_limiar.resultado == META_SEM_DADO ||
        a->mare_limiar.resultado == META_SEM_DADO)
        return RISCO_INDETERMINADO;
    if (acima(&a->chuva_limiar) && acima(&a->mare_limiar))
        return RISCO_OCORRENDO;
    if (acima(&a->chuva_margem) && acima(&a->mare_margem))
        return RISCO_POTENCIAL;
    return RISCO_NENHUM;
}

Analise analisador_executar(const DadosChuva *chuva, const DadosMare *mare,
                            Limiares lim)
{
    Analise a;
    int ok_chuva = chuva != NULL && chuva->disponivel;
    int ok_mare = mare != NULL && mare->disponivel;
    int chuva_dispensavel = lim.chuva_mm <= 0.0;
    double media = ok_chuva ? chuva_media(chuva) : 0.0;
    double h = ok_mare ? mare_altura_efetiva(mare) : 0.0;
    int usa_chuva = ok_chuva || chuva_dispensavel;

    memset(&a, 0, sizeof(a));
    a.limiares = lim;
    a.mare_m = h;
    a.chuva_limiar = analisador_avaliar(media, lim.chuva_mm, usa_chuva);
    a.chuva_margem = analisador_avaliar(media, lim.chuva_mm * MARGEM_POTENCIAL, usa_chuva);
    a.mare_limiar = analisador_avaliar(h, lim.mare_m, ok_mare);
    a.mare_margem = analisador_avaliar(h, lim.mare_m * MARGEM_POTENCIAL, ok_mare);
    a.risco = classificar_risco(&a);
    a.alerta = a.risco == RISCO_OCORRENDO;
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
