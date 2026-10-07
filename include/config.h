#ifndef CONFIG_H
#define CONFIG_H

#ifndef DATA_DIR
#define DATA_DIR "../data"
#endif

#define APAC_URL "http://dados.apac.pe.gov.br:41120/cemaden/"
#define CIDADE_ALVO "RECIFE"

#define ARQUIVO_MARE      DATA_DIR "/mare_recife.csv"
#define ARQUIVO_DRENAGEM  DATA_DIR "/drenagem_recife_antigo.csv"

#define INTERVALO_LEITURA 30
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

/* Metas do analisador (provisórias, validar na etapa experimental) */
#define META_CHUVA_ATENCAO_MM  0.1
#define META_CHUVA_CRITICA_MM  5.0
#define META_MARE_INTERMED_POS 0.25
#define META_MARE_ALTA_POS     0.75

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


#endif
