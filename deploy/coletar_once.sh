#!/bin/sh
# Coleta UMA leitura e sai. Para maquinas que ficam ligadas so as vezes ou para cron.
#   crontab -e  ->  */10 * * * * /caminho/backflow-draining-project/deploy/coletar_once.sh >> /tmp/coleta.log 2>&1
# flock evita duas coletas simultaneas; o programa grava no mesmo data/leituras_log.csv.
RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
cd "$RAIZ/build" || exit 1
exec flock -n /tmp/backflow_coleta.lock ./device_recife --once
