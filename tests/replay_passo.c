#include <stdio.h>
#include "adaptador.h"

int main(void)
{
    double lr, lh, r, h, peso;
    int alerta, houve;

    while (scanf("%lf %lf %lf %lf %d %d %lf", &lr, &lh, &r, &h, &alerta, &houve, &peso) == 7) {
        Limiares l = {lr, lh};
        Limiares n = adaptador_passo(l, r, h, alerta, houve, peso);

        printf("%.12f %.12f\n", n.chuva_mm, n.mare_m);
    }
    return 0;
}
