#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "estimacao_mare.h"

#include <curl/curl.h>
#include <cjson/cJSON.h>


/*
 * ============================================================
 * SENSOR VIRTUAL RECIFE
 * ============================================================
 *
 * Projeto: IN1061 - Tópicos Avançados em Sistemas Distribuídos 2
 *
 * Objetivo:
 * Integrar dados de chuva e maré para investigar condições
 * associadas à ocorrência de alagamentos no Recife.
 *
 * Entradas:
 *   - Chuva: APAC/CEMADEN
 *   - Maré: previsão oficial do CHM para o Porto do Recife
 *
 * Estação maregráfica:
 *   Porto do Recife
 *
 * A saída apresenta uma interpretação da condição ambiental.
 * Os dados de entrada são obtidos das fontes externas; a indicação
 * de alagamento/refluxo é inferencial e não substitui medição física.
 * Os limiares de chuva usados nesta versão são provisórios e deverão
 * ser validados/substituídos na etapa experimental.
 *
 * ============================================================
 */


/* ============================================================
 * CONFIGURAÇÕES
 * ============================================================
 */

/* Fonte de dados de chuva */
#define APAC_URL "http://dados.apac.pe.gov.br:41120/cemaden/"

/*
 * Arquivo contendo os eventos previstos da Tábua de Marés
 * do Porto do Recife - 2026.
 *
 * Formato:
 *
 * data,hora,altura
 * 2026-09-26,03:19,2.44
 * 2026-09-26,09:34,0.13
 */
#define ARQUIVO_MARE "../data/mare_recife.csv"

/* Arquivo com os elementos de drenagem do Bairro do Recife */
#define ARQUIVO_DRENAGEM "../data/drenagem_recife_antigo.csv"

/* Intervalo entre as leituras */
#define INTERVALO_LEITURA 30


/* ============================================================
 * ESTRUTURAS
 * ============================================================
 */

typedef struct
{
    double soma_chuva;

    double maior_chuva;

    int total_estacoes;

    char ultima_leitura[64];

} DadosChuva;


typedef struct
{
    int disponivel;

    char data[32];

    char hora[32];

    double altura;

    double menor_altura_dia;

    double maior_altura_dia;

    long diferenca_minutos;

} DadosMare;


typedef struct
{
    int bocas_lobo;
    int pocos_visita;
    int caixas_gaveta;
    int total_elementos;

} DadosDrenagem;


typedef struct
{
    double chuva_media;

    int estacoes;

    DadosMare mare;

    DadosDrenagem drenagem;

    char timestamp[64];

} LeituraVirtual;


/* ============================================================
 * BUFFER DO CURL
 * ============================================================
 */

typedef struct
{
    char *dados;

    size_t tamanho;

} Buffer;


/* ============================================================
 * CALLBACK DO CURL
 * ============================================================
 */

static size_t write_cb(
    void *contents,
    size_t size,
    size_t nmemb,
    void *userp
)
{
    size_t tamanho_total;

    Buffer *buffer;

    char *novo_buffer;


    tamanho_total = size * nmemb;

    buffer = (Buffer *)userp;


    novo_buffer = realloc(
        buffer->dados,
        buffer->tamanho + tamanho_total + 1
    );


    if (novo_buffer == NULL)
    {
        fprintf(
            stderr,
            "Erro: memoria insuficiente.\n"
        );

        return 0;
    }


    buffer->dados = novo_buffer;


    memcpy(
        buffer->dados + buffer->tamanho,
        contents,
        tamanho_total
    );


    buffer->tamanho += tamanho_total;

    buffer->dados[buffer->tamanho] = '\0';


    return tamanho_total;
}


/* ============================================================
 * REQUISIÇÃO HTTP
 * ============================================================
 */

static char *fetch_payload(const char *url)
{
    CURL *curl;

    CURLcode resultado;

    Buffer buffer;


    buffer.dados = NULL;

    buffer.tamanho = 0;


    curl = curl_easy_init();


    if (curl == NULL)
    {
        fprintf(
            stderr,
            "Erro ao inicializar CURL.\n"
        );

        return NULL;
    }


    curl_easy_setopt(
        curl,
        CURLOPT_URL,
        url
    );


    curl_easy_setopt(
        curl,
        CURLOPT_FOLLOWLOCATION,
        1L
    );


    curl_easy_setopt(
        curl,
        CURLOPT_CONNECTTIMEOUT,
        10L
    );


    curl_easy_setopt(
        curl,
        CURLOPT_TIMEOUT,
        30L
    );


    curl_easy_setopt(
        curl,
        CURLOPT_WRITEFUNCTION,
        write_cb
    );


    curl_easy_setopt(
        curl,
        CURLOPT_WRITEDATA,
        &buffer
    );


    resultado = curl_easy_perform(curl);


    if (resultado != CURLE_OK)
    {
        fprintf(
            stderr,
            "Erro na requisicao HTTP: %s\n",
            curl_easy_strerror(resultado)
        );


        curl_easy_cleanup(curl);

        free(buffer.dados);

        return NULL;
    }


    curl_easy_cleanup(curl);


    return buffer.dados;
}


/* ============================================================
 * PROCESSAMENTO DA CHUVA
 * ============================================================
 */

static int processar_chuva_recife(
    const char *raw_json,
    DadosChuva *resultado
)
{
    cJSON *root;

    cJSON *item;

    int quantidade;

    double soma;

    long long ultima_data_hora;


    if (
        raw_json == NULL ||
        resultado == NULL
    )
    {
        return 0;
    }


    resultado->soma_chuva = 0.0;

    resultado->maior_chuva = 0.0;

    resultado->total_estacoes = 0;

    resultado->ultima_leitura[0] = '\0';


    quantidade = 0;

    soma = 0.0;

    ultima_data_hora = -1;


    root = cJSON_Parse(raw_json);


    if (root == NULL)
    {
        fprintf(
            stderr,
            "Erro ao interpretar JSON da APAC.\n"
        );

        return 0;
    }


    if (!cJSON_IsArray(root))
    {
        fprintf(
            stderr,
            "Erro: resposta da APAC nao e um array JSON.\n"
        );

        cJSON_Delete(root);

        return 0;
    }


    cJSON_ArrayForEach(item, root)
    {
        cJSON *dados_completos;

        cJSON *dados_json;

        cJSON *cidade;

        cJSON *chuva;

        cJSON *data_hora;


        dados_completos =
            cJSON_GetObjectItemCaseSensitive(
                item,
                "Dados_completos"
            );


        if (!cJSON_IsString(dados_completos))
        {
            continue;
        }


        dados_json =
            cJSON_Parse(
                dados_completos->valuestring
            );


        if (dados_json == NULL)
        {
            continue;
        }


        cidade =
            cJSON_GetObjectItemCaseSensitive(
                dados_json,
                "cidade"
            );


        if (
            cidade != NULL &&
            cJSON_IsString(cidade) &&
            strcmp(
                cidade->valuestring,
                "RECIFE"
            ) == 0
        )
        {
            chuva =
                cJSON_GetObjectItemCaseSensitive(
                    dados_json,
                    "chuva"
                );


            if (
                chuva != NULL &&
                cJSON_IsNumber(chuva)
            )
            {
                soma += chuva->valuedouble;

                if (chuva->valuedouble > resultado->maior_chuva)
                {
                    resultado->maior_chuva = chuva->valuedouble;
                }

                quantidade++;
            }


            data_hora =
                cJSON_GetObjectItemCaseSensitive(
                    dados_json,
                    "dataHora"
                );


            if (
                data_hora != NULL &&
                cJSON_IsString(data_hora)
            )
            {
                int ano;
                int mes;
                int dia;
                int hora;
                int minuto;
                long long valor_data_hora;

                if (sscanf(
                    data_hora->valuestring,
                    "%d-%d-%d %d:%d",
                    &ano,
                    &mes,
                    &dia,
                    &hora,
                    &minuto
                ) == 5)
                {
                    valor_data_hora =
                        ((long long)ano * 100000000LL) +
                        ((long long)mes * 1000000LL) +
                        ((long long)dia * 10000LL) +
                        ((long long)hora * 100LL) +
                        (long long)minuto;

                    {
                        if (valor_data_hora >= ultima_data_hora)
                        {
                            ultima_data_hora = valor_data_hora;

                            strncpy(
                                resultado->ultima_leitura,
                                data_hora->valuestring,
                                sizeof(
                                    resultado->ultima_leitura
                                ) - 1
                            );

                            resultado->ultima_leitura[
                                sizeof(
                                    resultado->ultima_leitura
                                ) - 1
                            ] = '\0';
                        }
                    }
                }
            }
        }


        cJSON_Delete(dados_json);
    }


    cJSON_Delete(root);


    resultado->soma_chuva = soma;

    resultado->total_estacoes = quantidade;


    return quantidade > 0;
}


/* ============================================================
 * CONVERSÃO DE DATA E HORA
 * ============================================================
 *
 * Converte:
 *
 *     YYYY-MM-DD
 *     HH:MM
 *
 * em um número utilizado somente para comparar
 * qual observação é mais recente.
 *
 * ============================================================
 */

static long long converter_data_hora(
    const char *data,
    const char *hora
)
{
    int ano;

    int mes;

    int dia;

    int h;

    int minuto;


    if (
        data == NULL ||
        hora == NULL
    )
    {
        return -1;
    }


    if (
        sscanf(
            data,
            "%d-%d-%d",
            &ano,
            &mes,
            &dia
        ) != 3
    )
    {
        return -1;
    }


    if (
        sscanf(
            hora,
            "%d:%d",
            &h,
            &minuto
        ) != 2
    )
    {
        return -1;
    }


    return
        ((long long)ano * 100000000LL) +
        ((long long)mes * 1000000LL) +
        ((long long)dia * 10000LL) +
        ((long long)h * 100LL) +
        (long long)minuto;
}


/* ============================================================
 * LEITURA DA TÁBUA DE MARÉS
 * ============================================================
 *
 * Fonte:
 *   Marinha do Brasil / CHM
 *   Tábua de Marés - Porto do Recife - 2026
 *
 * O arquivo CSV contém os eventos previstos de maré:
 *
 *   data,hora,altura
 *
 * A função percorre a tabela e seleciona o evento de maré
 * temporalmente mais próximo do momento da leitura.
 *
 * IMPORTANTE:
 *   Os valores desta etapa são previsões da tábua de marés,
 *   e não observações maregráficas em tempo real.
 *
 * ============================================================
 */

static int obter_mare_recife(
    const char *arquivo,
    DadosMare *mare
)
{
    FILE *arquivo_mare;
    char linha[256];

    time_t agora;
    double menor_diferenca;
    int encontrou;

    double menor_altura_dia;
    double maior_altura_dia;
    char data_referencia[32];

    if (
        arquivo == NULL ||
        mare == NULL
    )
    {
        return 0;
    }

    mare->disponivel = 0;
    mare->altura = 0.0;
    mare->menor_altura_dia = 0.0;
    mare->maior_altura_dia = 0.0;
    mare->diferenca_minutos = 0;
    mare->data[0] = '\0';
    mare->hora[0] = '\0';

    agora = time(NULL);
    menor_diferenca = -1.0;
    encontrou = 0;

    menor_altura_dia = 0.0;
    maior_altura_dia = 0.0;
    data_referencia[0] = '\0';

    arquivo_mare = fopen(
        arquivo,
        "r"
    );

    if (arquivo_mare == NULL)
    {
        fprintf(
            stderr,
            "Nao foi possivel abrir a tabela de mare: %s\n",
            arquivo
        );

        return 0;
    }

    fgets(
        linha,
        sizeof(linha),
        arquivo_mare
    );

    while (
        fgets(
            linha,
            sizeof(linha),
            arquivo_mare
        ) != NULL
    )
    {
        char data[32];
        char hora[32];
        double altura;

        int ano;
        int mes;
        int dia;
        int hora_evento;
        int minuto_evento;

        struct tm tempo_mare;
        time_t instante_mare;
        double diferenca;

        if (
            sscanf(
                linha,
                " %31[^,],%31[^,],%lf",
                data,
                hora,
                &altura
            ) != 3
        )
        {
            continue;
        }

        if (
            sscanf(
                data,
                "%d-%d-%d",
                &ano,
                &mes,
                &dia
            ) != 3
        )
        {
            continue;
        }

        if (
            sscanf(
                hora,
                "%d:%d",
                &hora_evento,
                &minuto_evento
            ) != 2
        )
        {
            continue;
        }

        /*
         * Primeiro, calcula a faixa de alturas prevista para o
         * mesmo dia do evento mais proximo da leitura.
         */
        if (data_referencia[0] == '\0')
        {
            strncpy(
                data_referencia,
                data,
                sizeof(data_referencia) - 1
            );
            data_referencia[sizeof(data_referencia) - 1] = '\0';
            menor_altura_dia = altura;
            maior_altura_dia = altura;
        }

        memset(
            &tempo_mare,
            0,
            sizeof(tempo_mare)
        );

        tempo_mare.tm_year = ano - 1900;
        tempo_mare.tm_mon = mes - 1;
        tempo_mare.tm_mday = dia;
        tempo_mare.tm_hour = hora_evento;
        tempo_mare.tm_min = minuto_evento;
        tempo_mare.tm_sec = 0;

        instante_mare = mktime(
            &tempo_mare
        );

        if (instante_mare == (time_t)-1)
        {
            continue;
        }

        diferenca = difftime(
            instante_mare,
            agora
        );

        if (diferenca < 0)
        {
            diferenca = -diferenca;
        }

        if (
            !encontrou ||
            diferenca < menor_diferenca
        )
        {
            menor_diferenca = diferenca;

            strncpy(
                mare->data,
                data,
                sizeof(mare->data) - 1
            );

            mare->data[
                sizeof(mare->data) - 1
            ] = '\0';

            strncpy(
                mare->hora,
                hora,
                sizeof(mare->hora) - 1
            );

            mare->hora[
                sizeof(mare->hora) - 1
            ] = '\0';

            mare->altura = altura;
            encontrou = 1;
        }
    }

    fclose(arquivo_mare);

    if (!encontrou)
    {
        return 0;
    }

    /*
     * Segunda passagem: calcula a faixa de maré do dia do evento
     * selecionado. Isso permite uma classificação relativa baseada
     * na própria tábua, sem inventar uma altura atual.
     */
    arquivo_mare = fopen(
        arquivo,
        "r"
    );

    if (arquivo_mare != NULL)
    {
        fgets(linha, sizeof(linha), arquivo_mare);

        menor_altura_dia = mare->altura;
        maior_altura_dia = mare->altura;

        while (fgets(linha, sizeof(linha), arquivo_mare) != NULL)
        {
            char data[32];
            char hora[32];
            double altura;

            if (
                sscanf(
                    linha,
                    " %31[^,],%31[^,],%lf",
                    data,
                    hora,
                    &altura
                ) != 3
            )
            {
                continue;
            }

            if (strcmp(data, mare->data) == 0)
            {
                if (altura < menor_altura_dia)
                    menor_altura_dia = altura;

                if (altura > maior_altura_dia)
                    maior_altura_dia = altura;
            }
        }

        fclose(arquivo_mare);
    }

    mare->menor_altura_dia = menor_altura_dia;
    mare->maior_altura_dia = maior_altura_dia;

    mare->diferenca_minutos =
        (long)(menor_diferenca / 60.0 + 0.5);

    mare->disponivel = 1;

    return 1;
}




/* ============================================================
 * LEITURA DA INFRAESTRUTURA DE DRENAGEM
 * ============================================================
 *
 * Fonte:
 *   data/drenagem_recife_antigo.csv
 *
 * O arquivo contém os elementos de drenagem localizados
 * no Bairro do Recife. Esses dados são estáticos e servem
 * como contexto da infraestrutura para o Sensor Virtual.
 *
 * Formato:
 *   id,tipo,latitude,longitude
 *
 * Os códigos de tipo são contabilizados diretamente do CSV:
 *   BL = elemento do tipo BL
 *   PV = elemento do tipo PV
 *   CG = elemento do tipo CG
 *
 * ============================================================
 */

static int carregar_drenagem_recife(
    const char *arquivo,
    DadosDrenagem *drenagem
)
{
    FILE *arquivo_drenagem;
    char linha[256];
    int carregou;

    if (
        arquivo == NULL ||
        drenagem == NULL
    )
    {
        return 0;
    }

    drenagem->bocas_lobo = 0;
    drenagem->pocos_visita = 0;
    drenagem->caixas_gaveta = 0;
    drenagem->total_elementos = 0;

    arquivo_drenagem = fopen(
        arquivo,
        "r"
    );

    if (arquivo_drenagem == NULL)
    {
        fprintf(
            stderr,
            "Nao foi possivel abrir o arquivo de drenagem: %s\n",
            arquivo
        );

        return 0;
    }

    /* Ignora o cabecalho */
    fgets(
        linha,
        sizeof(linha),
        arquivo_drenagem
    );

    carregou = 0;

    while (
        fgets(
            linha,
            sizeof(linha),
            arquivo_drenagem
        ) != NULL
    )
    {
        char id[32];
        char tipo[32];
        char latitude[64];
        char longitude[64];

        if (
            sscanf(
                linha,
                " %31[^,],%31[^,],%63[^,],%63[^\n\r]",
                id,
                tipo,
                latitude,
                longitude
            ) != 4
        )
        {
            continue;
        }

        if (strcmp(tipo, "BL") == 0)
        {
            drenagem->bocas_lobo++;
        }
        else if (strcmp(tipo, "PV") == 0)
        {
            drenagem->pocos_visita++;
        }
        else if (strcmp(tipo, "CG") == 0)
        {
            drenagem->caixas_gaveta++;
        }
        else
        {
            continue;
        }

        drenagem->total_elementos++;
        carregou = 1;
    }

    fclose(arquivo_drenagem);

    return carregou;
}


/* ============================================================
 * TIMESTAMP DO SISTEMA
 * ============================================================
 */

static void gerar_timestamp(
    char *buffer,
    size_t tamanho
)
{
    time_t agora;

    struct tm *tempo_local;


    agora = time(NULL);


    tempo_local =
        localtime(&agora);


    if (tempo_local == NULL)
    {
        snprintf(
            buffer,
            tamanho,
            "timestamp-indisponivel"
        );

        return;
    }


    strftime(
        buffer,
        tamanho,
        "%Y-%m-%d %H:%M:%S",
        tempo_local
    );
}


/* ============================================================
 * INTERPRETAÇÃO DA SITUAÇÃO
 * ============================================================
 *
 * Os valores de chuva e maré continuam sendo obtidos dos dados
 * externos. As funções abaixo apenas transformam esses valores
 * em uma descrição legível.
 *
 * IMPORTANTE:
 * A indicação de alagamento/refluxo é uma indicação baseada na
 * combinação das condições ambientais. Ela NÃO é uma detecção
 * física de refluxo, pois o projeto não possui sensor de nível
 * instalado na drenagem.
 *
 * Os limiares de chuva abaixo são provisórios e devem ser
 * substituídos pelos limiares derivados na etapa experimental.
 * ============================================================
 */

static const char *classificar_chuva(double chuva_media)
{
    if (chuva_media <= 0.0)
    {
        return "BAIXA";
    }

    if (chuva_media < 5.0)
    {
        return "ATENÇÃO";
    }

    return "CRÍTICA";
}


static const char *classificar_mare(const DadosMare *mare)
{
    double faixa;
    double posicao;

    if (mare == NULL || !mare->disponivel)
    {
        return "INDISPONÍVEL";
    }

    faixa =
        mare->maior_altura_dia -
        mare->menor_altura_dia;

    if (faixa <= 0.0)
    {
        return "INDEFINIDA";
    }

    posicao =
        (mare->altura - mare->menor_altura_dia) /
        faixa;

    if (posicao >= 0.75)
    {
        return "ALTA";
    }

    if (posicao <= 0.25)
    {
        return "BAIXA";
    }

    return "INTERMEDIÁRIA";
}


static const char *classificar_ambiente(
    const char *condicao_chuva,
    const char *condicao_mare
)
{
    if (
        strcmp(condicao_chuva, "CRÍTICA") == 0 ||
        strcmp(condicao_mare, "ALTA") == 0
    )
    {
        return "CRÍTICA";
    }

    if (
        strcmp(condicao_chuva, "ATENÇÃO") == 0 ||
        strcmp(condicao_mare, "INTERMEDIÁRIA") == 0
    )
    {
        return "ATENÇÃO";
    }

    if (
        strcmp(condicao_chuva, "INDISPONÍVEL") == 0 ||
        strcmp(condicao_mare, "INDISPONÍVEL") == 0 ||
        strcmp(condicao_mare, "INDEFINIDA") == 0
    )
    {
        return "DADOS INSUFICIENTES";
    }

    return "NORMAL";
}


static const char *indicar_alagamento(
    const char *condicao_chuva,
    const char *condicao_mare
)
{
    if (
        strcmp(condicao_chuva, "CRÍTICA") == 0 &&
        strcmp(condicao_mare, "ALTA") == 0
    )
    {
        return "POSSÍVEL";
    }

    return "NÃO IDENTIFICADA";
}


static const char *indicar_refluxo(
    const char *condicao_chuva,
    const char *condicao_mare
)
{
    if (
        strcmp(condicao_chuva, "CRÍTICA") == 0 &&
        strcmp(condicao_mare, "ALTA") == 0
    )
    {
        return "CONDIÇÃO COMPATÍVEL";
    }

    return "NÃO IDENTIFICADA";
}


/* ============================================================
 * CICLO DE LEITURA
 * ============================================================
 */

static void ciclo_de_leitura(const DadosDrenagem *drenagem)
{
    char *payload;

    DadosChuva chuva;

    DadosMare mare;

    LeituraVirtual leitura;

    double chuva_media;

    const char *condicao_chuva;
    const char *condicao_mare;
    const char *condicao_ambiental;
    const char *indicacao_alagamento;
    const char *indicacao_refluxo;

    printf("\n");

    printf(
        "============================================\n"
    );

    printf(
        "SENSOR VIRTUAL RECIFE\n"
    );

    printf(
        "============================================\n"
    );


    /* --------------------------------------------------------
     * 1. CONSULTA APAC
     * --------------------------------------------------------
     */

    printf(
        "[1/4] Consultando APAC/CEMADEN...\n"
    );


    payload =
        fetch_payload(
            APAC_URL
        );


    if (payload == NULL)
    {
        fprintf(
            stderr,
            "Falha ao obter dados da APAC.\n"
        );

        return;
    }


    /* --------------------------------------------------------
     * 2. PROCESSA CHUVA
     * --------------------------------------------------------
     */

    printf(
        "[2/4] Processando dados de chuva...\n"
    );


    if (
        !processar_chuva_recife(
            payload,
            &chuva
        )
    )
    {
        fprintf(
            stderr,
            "Nao foi possivel obter dados de chuva para Recife.\n"
        );

        free(payload);

        return;
    }


    free(payload);


    chuva_media = 0.0;


    if (chuva.total_estacoes > 0)
    {
        chuva_media =
            chuva.soma_chuva /
            chuva.total_estacoes;
    }


    /* --------------------------------------------------------
     * 3. CONSULTA TÁBUA DE MARÉS
     * --------------------------------------------------------
     */

    printf(
        "[3/4] Consultando tabela de mare do Porto do Recife...\n"
    );


    obter_mare_recife(
        ARQUIVO_MARE,
        &mare
    );


    printf(
        "[4/4] Consultando infraestrutura do Recife Antigo...\n"
    );


    /* --------------------------------------------------------
     * INTEGRA SENSOR VIRTUAL
     * --------------------------------------------------------
     */

    leitura.chuva_media =
        chuva_media;


    leitura.estacoes =
        chuva.total_estacoes;


    leitura.mare =
        mare;

    if (drenagem != NULL)
    {
        leitura.drenagem = *drenagem;
    }
    else
    {
        memset(
            &leitura.drenagem,
            0,
            sizeof(leitura.drenagem)
        );
    }


    gerar_timestamp(
        leitura.timestamp,
        sizeof(leitura.timestamp)
    );


    condicao_chuva =
        classificar_chuva(leitura.chuva_media);

    condicao_mare =
        classificar_mare(&leitura.mare);

    condicao_ambiental =
        classificar_ambiente(
            condicao_chuva,
            condicao_mare
        );

    indicacao_alagamento =
        indicar_alagamento(
            condicao_chuva,
            condicao_mare
        );

    indicacao_refluxo =
        indicar_refluxo(
            condicao_chuva,
            condicao_mare
        );


    /* --------------------------------------------------------
     * EXIBE RESULTADO
     * --------------------------------------------------------
     */

    printf("\n");

    printf(
        "--------------------------------------------\n"
    );

    printf(
        "LEITURA DO SENSOR VIRTUAL\n"
    );

    printf(
        "--------------------------------------------\n"
    );


    printf(
        "Timestamp do sistema : %s\n",
        leitura.timestamp
    );


    printf(
        "Chuva media          : %.2f\n",
        leitura.chuva_media
    );


    printf(
        "Estacoes consideradas: %d\n",
        leitura.estacoes
    );


    if (
        chuva.ultima_leitura[0] != '\0'
    )
    {
        printf(
            "Ultima leitura APAC  : %s\n",
            chuva.ultima_leitura
        );
    }
    else
    {
        printf(
            "Ultima leitura APAC  : indisponivel\n"
        );
    }


    if (
        leitura.mare.disponivel
    )
    {
        printf(
            "Mare prevista        : %.2f m\n",
            leitura.mare.altura
        );


        printf(
            "Data do evento       : %s\n",
            leitura.mare.data
        );


        printf(
            "Hora do evento       : %s\n",
            leitura.mare.hora
        );


        printf(
            "Fonte da mare        : Marinha do Brasil / CHM\n"
        );


        printf(
            "Estacao maregrafica  : Porto do Recife\n"
        );

        printf(
            "Faixa prevista no dia: %.2f a %.2f m\n",
            leitura.mare.menor_altura_dia,
            leitura.mare.maior_altura_dia
        );

        printf(
            "Evento mais proximo  : %ld min\n",
            leitura.mare.diferenca_minutos
        );
    }
    else
    {
        printf(
            "Mare prevista        : indisponivel\n"
        );


        printf(
            "Fonte da mare        : Marinha do Brasil / CHM\n"
        );


        printf(
            "Estacao maregrafica  : Porto do Recife\n"
        );
    }


    printf(
        "\n"
    );

    printf(
        "INFRAESTRUTURA DE DRENAGEM\n"
    );

    printf(
        "Bocas de lobo        : %d\n",
        leitura.drenagem.bocas_lobo
    );

    printf(
        "Pocos de visita      : %d\n",
        leitura.drenagem.pocos_visita
    );

    printf(
        "Caixas de gaveta     : %d\n",
        leitura.drenagem.caixas_gaveta
    );

    printf(
        "Total de elementos   : %d\n",
        leitura.drenagem.total_elementos
    );


    printf(
        "--------------------------------------------\n"
    );

    printf(
        "SITUACAO DO RECIFE ANTIGO\n"
    );

    printf(
        "--------------------------------------------\n"
    );

    printf(
        "Condicao de chuva       : %s\n",
        condicao_chuva
    );

    printf(
        "Condicao de mare        : %s\n",
        condicao_mare
    );

    printf(
        "Condicao ambiental      : %s\n",
        condicao_ambiental
    );

    printf(
        "Indicacao de alagamento : %s\n",
        indicacao_alagamento
    );

    printf(
        "Indicacao de refluxo    : %s\n",
        indicacao_refluxo
    );

    printf(
        "--------------------------------------------\n"
    );

    printf(
        "RESUMO\n"
    );

    printf(
        "--------------------------------------------\n"
    );

    printf(
        "Situacao atual          : %s\n",
        condicao_ambiental
    );

    printf(
        "Alagamento/refluxo      : %s / %s\n",
        indicacao_alagamento,
        indicacao_refluxo
    );


    printf(
        "Proxima leitura em %d segundos...\n",
        INTERVALO_LEITURA
    );


    printf(
        "============================================\n"
    );
}

/* monitorar usando a serie de fourier pra prever a mare de maneira continua */
void monitorar_ambiente(void) {
    time_t agora = time(NULL);

     
    ResultadoMare mare = calcular_sensor_mare(agora);

    /* Uso dos dados calculados no sensor virtual, basicamente é a exposicao da struct */
    printf("\n--- SENSOR VIRTUAL DE MARÉ ---");
    printf("\nAltura Estimada : %.2f m", mare.altura_m);
    printf("\nTendência       : %.2f m/h", mare.velocidade_m_h);
    printf("\nEstado Atual    : %s", mare.estado);
    printf("\n-------------------------------\n");

    /* Aplicação na lógica de risco do Recife Antigo */
    if (mare.altura_m >= 2.0 && mare.enchente) {
        printf("[ALERTA] Risco elevado de refluxo de maré nas galerias pluviais!\n");
    }
}


/* ============================================================
 * FUNÇÃO PRINCIPAL
 * ============================================================
 */

int main(void)
{   
    
    CURLcode resultado_curl;

    DadosDrenagem drenagem;


    resultado_curl =
        curl_global_init(
            CURL_GLOBAL_DEFAULT
        );


    if (
        resultado_curl != CURLE_OK
    )
    {
        fprintf(
            stderr,
            "Erro ao inicializar libcurl.\n"
        );

        return EXIT_FAILURE;
    }


    printf("\n");

    printf(
        "============================================\n"
    );

    printf(
        "SENSOR VIRTUAL RECIFE - IN1061\n"
    );

    printf(
        "Chuva APAC + Mare prevista (CHM)\n"
    );

    printf(
        "============================================\n"
    );


    printf(
        "Carregando infraestrutura de drenagem...\n"
    );

    if (!carregar_drenagem_recife(ARQUIVO_DRENAGEM, &drenagem))
    {
        fprintf(
            stderr,
            "Falha ao carregar a infraestrutura de drenagem.\n"
        );

        curl_global_cleanup();

        return EXIT_FAILURE;
    }

    printf(
        "Elementos de drenagem carregados: %d\n",
        drenagem.total_elementos
    );



    
    /*
     * Executa continuamente.
     */
    while (1)
    {   
        ciclo_de_leitura(&drenagem);

        //caso queira testar a estimação de maré matematicamente continua descomente abaixo :P
        //monitorar_ambiente();

        

        sleep(
            INTERVALO_LEITURA
        );
    }


    curl_global_cleanup();

    return EXIT_SUCCESS;
}