#ifndef ADAPTADOR_H
#define ADAPTADOR_H

#include "limiares.h"

typedef enum { FB_TN, FB_TP, FB_FP, FB_FN } TipoFeedback;

typedef struct {
    TipoFeedback tipo;
    Limiares antes;
    Limiares depois;
} ResultadoFeedback;

typedef struct {
    unsigned long tp, fp, fn, tn, atualizacoes, fn_ignorados;
} ContagemFeedback;

Limiares adaptador_passo(Limiares lim, double chuva_mm, double mare_m,
                         int alerta, int houve, double peso);
TipoFeedback adaptador_classificar(int alerta, int houve);

void adaptador_iniciar(void);
Limiares adaptador_limiares(void);
ContagemFeedback adaptador_contagens(void);
ResultadoFeedback adaptador_aplicar(double chuva_mm, double mare_m,
                                    int alerta, int houve, double peso);
const char *adaptador_nome_tipo(TipoFeedback t);

#endif
