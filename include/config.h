#ifndef CONFIG_H
#define CONFIG_H

#ifndef DATA_DIR
#define DATA_DIR "../data"
#endif

#ifndef APAC_URL
#define APAC_URL "http://dados.apac.pe.gov.br:41120/cemaden/"
#endif
#define APAC_DATAHORA_UTC 0
#define CIDADE_ALVO "RECIFE"

#define ARQUIVO_MARE      DATA_DIR "/mare_recife.csv"
#define ARQUIVO_DRENAGEM  DATA_DIR "/drenagem_recife_antigo.csv"

#ifndef INTERVALO_LEITURA
#define INTERVALO_LEITURA 30
#endif
#define LINHA_MAX 256

#define HTTP_CONNECT_TIMEOUT_S 10L
#define HTTP_TIMEOUT_S 30L

#define JSON_CAMPO_ENVELOPE "Dados_completos"
#define JSON_CAMPO_CIDADE   "cidade"
#define JSON_CAMPO_CHUVA    "chuva"
#define JSON_CAMPO_DATAHORA "dataHora"

#define DRENAGEM_TIPO_BL "BL"
#define DRENAGEM_TIPO_PV "PV"
#define DRENAGEM_TIPO_CG "CG"

#define Z0_RECIFE 1.35
#define HABILITAR_MARE_HARMONICA 0

/* Limiares iniciais (provisorios). Condicao de refluxo: chuva >= LIMIAR_CHUVA
   e mare >= LIMIAR_MARE (altura em metros, datum DHN). Se o adaptador levar
   o limiar de chuva a 0, o sensor passa a operar so com a mare. */
#define LIMIAR_CHUVA_INICIAL_MM 5.0
#define LIMIAR_MARE_INICIAL_M   2.0
#define LIMIAR_CHUVA_MIN_MM 0.0
#define LIMIAR_CHUVA_MAX_MM 50.0
#define LIMIAR_MARE_MIN_M   1.0
#define LIMIAR_MARE_MAX_M   2.7
#define MARGEM_POTENCIAL 0.7

#define MARE_DIF_MAX_MIN 480
#define MARE_GAP_MAX_S (10 * 3600)

#define ADAPT_HABILITADO 1
#define ADAPT_TAXA 0.3
#define ADAPT_PESO_PERDA 2.0
#define ADAPT_MARGEM 0.02
#define ADAPT_RAIO_CONFIANCA 0.15
#define ARQUIVO_ESTADO_LIMIARES DATA_DIR "/limiares_estado.csv"

#define MQTT_HABILITADO 1
#define MQTT_HOST "localhost"
#define MQTT_PORTA 1883
#define MQTT_KEEPALIVE_S 60
#define MQTT_QOS 1
#define MQTT_CLIENT_ID "sensor-virtual-recife"
#define MQTT_USUARIO ""
#define MQTT_SENHA ""
#define MQTT_TOPICO_LEITURA "backflow/recife/leitura"
#define MQTT_TOPICO_STATUS  "backflow/recife/status"
#define MQTT_TOPICO_FEEDBACK "backflow/recife/feedback"

#define FEEDBACK_HABILITADO 1
#define FEEDBACK_VALIDADE_S 3600
#define FEEDBACK_MAX_POR_USUARIO_HORA 6
#define FEEDBACK_MAX_ATUALIZACOES_HORA 12
#define PERGUNTA_INTERVALO_MIN_S 900
#define PERGUNTA_EPSILON 0.02
#define FEEDBACK_SEMENTE 0
#define ARQUIVO_FEEDBACK DATA_DIR "/feedback_log.csv"


#define LOG_HABILITADO 1
#define ARQUIVO_LOG DATA_DIR "/leituras_log.csv"
#define LOG_BRUTO_HABILITADO 1
#define LOG_BRUTO_A_CADA 10
#define DIR_BRUTO DATA_DIR "/raw"

#endif
