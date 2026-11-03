#include <stdio.h>
#include <string.h>
#include "config.h"
#include "mare_tabua.h"
#include "log_csv.h"

static const char CABECALHO[] =
    "epoch,boot_id,seq,timestamp,chuva_ok,chuva_media_mm,chuva_maior_mm,"
    "estacoes,chuva_ultima_leitura,mare_ok,mare_evento_data,mare_evento_hora,"
    "mare_evento_m,mare_interp_m,mare_min_dia_m,mare_max_dia_m,mare_dif_min,"
    "limiar_chuva_mm,limiar_mare_m,alerta,pergunta,risco_codigo,risco_nome,"
    "http_code,curl_rc,fetch_ms,connect_ms,ttfb_ms,bytes,idade_dado_s\n";

static void garantir_cabecalho(void)
{
    FILE *f = fopen(ARQUIVO_LOG, "r");
    char linha[1024];
    char destino[512];

    if (f == NULL)
        return;
    if (fgets(linha, sizeof(linha), f) == NULL || strcmp(linha, CABECALHO) == 0) {
        fclose(f);
        return;
    }
    fclose(f);
    snprintf(destino, sizeof(destino), "%s.antigo", ARQUIVO_LOG);
    if (rename(ARQUIVO_LOG, destino) == 0)
        fprintf(stderr, "Log: cabecalho diferente, arquivo anterior movido para %s\n", destino);
}

int log_csv_registrar(const LeituraVirtual *l)
{
    static int verificado;
    FILE *f;

    if (!verificado) {
        garantir_cabecalho();
        verificado = 1;
    }
    f = fopen(ARQUIVO_LOG, "a+");
    if (f == NULL) {
        fprintf(stderr, "Log: nao foi possivel abrir %s\n", ARQUIVO_LOG);
        return 0;
    }

    fseek(f, 0, SEEK_END);
    if (ftell(f) == 0)
        fputs(CABECALHO, f);

    fprintf(f, "%lld,%lld,%lu,%s,%d,%.4f,%.4f,%d,%s,%d,%s,%s,%.4f,%.4f,%.4f,%.4f,%ld,",
            (long long)l->epoch, l->boot_id, l->seq, l->timestamp,
            l->chuva.disponivel, l->chuva_media, l->chuva.maior_chuva,
            l->chuva.total_estacoes, l->chuva.ultima_leitura, l->mare.disponivel,
            l->mare.data, l->mare.hora, l->mare.altura, mare_altura_efetiva(&l->mare),
            l->mare.menor_altura_dia, l->mare.maior_altura_dia,
            l->mare.diferenca_minutos);
    fprintf(f, "%.4f,%.4f,%d,%d,%d,%s,%ld,%d,%.1f,%.1f,%.1f,%lu,%.1f\n",
            l->analise.limiares.chuva_mm, l->analise.limiares.mare_m,
            l->analise.alerta, l->pergunta, l->analise.risco,
            analisador_nome_risco(l->analise.risco), l->chuva.http_code,
            l->chuva.curl_rc, l->chuva.fetch_ms, l->chuva.connect_ms,
            l->chuva.ttfb_ms, (unsigned long)l->chuva.bytes, l->chuva.idade_dado_s);

    fclose(f);
    return 1;
}
