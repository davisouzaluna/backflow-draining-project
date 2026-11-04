# Protocolo experimental do sensor virtual

Este documento descreve como coletar dados com o sensor virtual de refluxo da drenagem (Recife), como verificar a qualidade da coleta e como conduzir a análise estatística. Os comandos assumem um sistema Linux (ou WSL) com o projeto compilado conforme o `README.md` da raiz.

## Sumário

1. [Estado de validação](#1-estado-de-validação)
2. [Fonte de dados de chuva](#2-fonte-de-dados-de-chuva)
3. [Primeira execução](#3-primeira-execução)
4. [Coleta contínua e redundante](#4-coleta-contínua-e-redundante)
5. [Checagem do experimento](#5-checagem-do-experimento)
6. [Plano de análise estatística](#6-plano-de-análise-estatística)
7. [Avaliação do mecanismo de feedback](#7-avaliação-do-mecanismo-de-feedback)
8. [Experimento de frescor](#8-experimento-de-frescor)
9. [Limitações conhecidas](#9-limitações-conhecidas)
10. [Referência de arquivos](#10-referência-de-arquivos)

---

## 1. Estado de validação

| Componente | Situação |
|---|---|
| Compilação e testes unitários (`ctest`) | Validado |
| Sensor lendo um feed simulado (`mock_apac.py`) | Validado |
| Publicação MQTT com broker local | Validado |
| Ajuste de limiares por feedback de usuários | Validado em simulação |
| Modo de leitura única (`--once`) | Validado |
| Checador do experimento | Detecta as falhas injetadas nos dados de teste |
| Leitura do feed real da APAC | Inspecionado; unidade e janela do campo `chuva` não confirmadas |
| Ocorrências reais de alagamento | Não disponíveis |

Sem ocorrências reais (`experimento/ocorrencias.csv`) não é possível medir acurácia. Até lá, a avaliação se limita à qualidade da coleta e ao comportamento do mecanismo.

Os arquivos em `experimento/sintetico/` e os resultados gerados a partir deles servem apenas para testar o código. Eles não representam o mundo real e não devem ser usados como resultado.

## 2. Fonte de dados de chuva

O feed da APAC/CEMADEN retorna uma lista de registros, cada um com um campo `Dados_completos` (JSON em texto) que contém `codestacao`, `cidade`, `chuva` e `dataHora`.

- O feed mantém cerca de 24 h de leituras por estação; há 16 estações no Recife.
- `dataHora` está em UTC (opção `APAC_DATAHORA_UTC` em `include/config.h`) e tem resolução de minuto.
- **A unidade e a janela de acumulação do campo `chuva` (10 min, 1 h ou 24 h) não estão confirmadas.** Os limiares de chuva só têm significado depois dessa confirmação.

Para inspecionar o feed e salvar uma amostra:

```bash
python3 scripts/inspecionar_apac.py --salvar data/raw/amostra.json
```

A seção "VEREDITO SOBRE HISTÓRICO" da saída indica se o feed traz uma série de leituras ou apenas a última. Para confirmar a unidade, compare o valor de uma estação com uma fonte independente (por exemplo, uma estação automática do INMET) durante um dia de chuva.

## 3. Primeira execução

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

O executável grava os dados em `../data` relativo ao diretório de execução (constante `DATA_DIR`). Execute-o a partir de `build/`:

```bash
cd build
./device_recife --once      # uma leitura e encerra
./device_recife             # laço contínuo (uma leitura a cada 30 s)
```

Se `idade_dado_s` no log apresentar um deslocamento fixo de horas, revise a opção `APAC_DATAHORA_UTC`.

## 4. Coleta contínua e redundante

O feed guarda cerca de 24 h por estação. Uma interrupção maior que isso em todos os coletores causa perda definitiva de dados. Por isso, a coleta não deve depender de uma única máquina, e toda interrupção deve ficar registrada nos dados.

### 4.1 Coletores

| Coletor | Mecanismo | Observação |
|---|---|---|
| Servidor principal (VM) | `deploy/sensor-virtual.service` (systemd, `Restart=always`) e `deploy/coletar_bruto.sh` | Reinicia sozinho após queda ou reboot. |
| Máquina secundária | `deploy/coletar_once.sh` no cron, ou o laço contínuo enquanto ligada | Use um diretório `data/` separado para não misturar o estado dos limiares. |
| GitHub Actions | `.github/workflows/coletar.yml`, a cada 15 min | Não testado. Pode atrasar ou pular execuções e precisa alcançar a porta 41120 da APAC. Use só como complemento. |

Os campos `boot_id` e `seq` do log identificam cada reinício e cada salto entre leituras. O log e o estado dos limiares são gravados de forma atômica.

### 4.2 Agendamento

Coleta do JSON bruto, a cada 6 h por máquina. Defina um nome diferente em `COLETOR` para cada máquina e escalone os minutos para que os coletores não executem juntos:

```cron
0 */6 * * *  COLETOR=vm-ufpe /caminho/backflow-draining-project/deploy/coletar_bruto.sh >> /tmp/coleta.log 2>&1
```

Leitura única do sensor, a cada 10 min (máquinas que não ficam ligadas o tempo todo):

```cron
*/10 * * * *  /caminho/backflow-draining-project/deploy/coletar_once.sh >> /tmp/coleta.log 2>&1
```

### 4.3 Unificação dos dados

Copie as pastas `data/raw_coletas/*` de cada máquina para uma pasta central e reconstrua o histórico de chuva. O script remove duplicatas entre máquinas e lista as lacunas:

```bash
python3 experimento/reconstruir_chuva.py PASTA_CENTRAL --saida data/chuva_historico.csv
```

Para unir os logs do sensor de vários coletores:

```bash
python3 experimento/mesclar_logs.py vm.csv pc.csv -o data/leituras_mescladas.csv --intervalo 600
```

Os campos de risco e alerta de cada coletor refletem os limiares daquele coletor. Na análise, use sempre a chuva e a maré brutas e recalcule o risco com os limiares do coletor principal.

## 5. Checagem do experimento

Execute a checagem diariamente para detectar cedo qualquer interrupção. A partir de `experimento/`:

```bash
python3 checar_experimento.py \
    --log ../data/leituras_log.csv \
    --tabua ../data/mare_recife.csv \
    [--ocorrencias ocorrencias.csv] \
    [--feedback ../data/feedback_log.csv]
```

O relatório é gravado em `resultados/checagem.md` e cobre três camadas.

### Camada A: a coleta é utilizável?

- Cobertura temporal, lacunas e reinícios.
- Cobertura por hora do dia.
- Cobertura por fase da maré e por intensidade de chuva. Se as lacunas se concentram nos momentos de chuva ou de maré alta, a amostra fica enviesada.
- Disponibilidade do feed e idade do dado.
- Plausibilidade física dos valores.

### Camada B: a implementação está correta?

- A maré calculada pelo sensor coincide com a maré recalculada em Python.
- O alerta registrado coincide com a regra recalculada a partir dos limiares do log.
- Nenhum alerta ocorre sem chuva (controle negativo).
- Fração do tempo com maré acima do limiar.

Complementam esta camada os testes unitários (`ctest`) e a verificação de paridade entre a regra em C e a regra em Python (seção 7).

### Camada C: o sensor acerta mais do que o acaso?

Requer o arquivo de ocorrências.

- Cobertura nos instantes das ocorrências.
- Placebo por deslocamento temporal: compara o CSI observado com o obtido após deslocar as ocorrências no tempo, o que preserva a autocorrelação da série.
- Comparação com as regras só-chuva e só-maré.
- Interação entre chuva e maré.

## 6. Plano de análise estatística

Congele este plano, com data, antes de examinar o período de teste.

| Item | Definição |
|---|---|
| Unidade de análise | Janela de 1 h. Sensibilidade com janelas de 30 min e 3 h, e antecedência de 0, 1 e 3 h. |
| Rótulo | Houve ocorrência registrada na janela. |
| Divisão dos dados | Temporal: os primeiros ~60% para calibração e o restante para teste com limiares congelados, ou validação *rolling-origin*. |
| Limiares no teste | Fixos. Com o feedback ativo, defina `ADAPT_HABILITADO 0` durante o teste. |
| Desfecho principal | TSS no período de teste, com IC95% por *bootstrap* em blocos de 24 janelas. |
| Desfechos secundários | POD, FAR, CSI, MCC, antecedência e desempenho por evento. |
| Hipótese central | Chuva e maré combinadas superam cada variável isolada: interação por regressão logística de Firth com teste da razão de verossimilhança, diferença de AUC por *bootstrap* em blocos e McNemar exato pareado, com correção de Holm. |

**Tamanho amostral.** São necessários cerca de 10 eventos por preditor (30 eventos para três preditores) e cerca de 60 eventos para um IC95% de largura 0,20 em POD = 0,8. Com menos eventos, reporte a regra simples com intervalos exatos e estudos de caso.

## 7. Avaliação do mecanismo de feedback

### 7.1 Simulação

A partir de `experimento/`:

```bash
python3 simulador_feedback.py --seeds 30 --paridade ../build/replay_passo
```

O simulador compara quatro políticas (estática, só confirmação de alertas, completa com exploração e relatos espontâneos, e oráculo) em dez cenários: limiar inicial alto ou baixo, deriva, poucos ou muitos usuários, ruído, e envenenamento aleatório ou dirigido. As políticas são comparadas pelo teste de Wilcoxon pareado por semente, com correção de Holm. A opção `--paridade` confirma que a regra em C e a regra em Python produzem o mesmo resultado (diferença < 10⁻⁹).

A simulação valida o mecanismo, não a sua utilidade com usuários reais.

### 7.2 Uso com usuários reais

Um piloto com pessoas exige avaliação ética prévia (Resolução CNS 510/2016). Para que o mecanismo tenha efeito, são necessárias dezenas de respostas por semana; na simulação, com cerca de 0,1 resposta por pergunta, o efeito é nulo.

## 8. Experimento de frescor

Este experimento não depende de ocorrências.

1. Com o sensor em execução, registre o instante de recebimento de cada mensagem (mesma máquina ou relógios sincronizados por NTP):

   ```bash
   python3 experimento/mqtt_logger.py --saida recebimentos.csv
   ```

2. Analise com as funções de `experimento/frescor.py`:

   | Função | Resultado |
   |---|---|
   | `resumo_frescor` | Idade média e de pico da informação (AoI) |
   | `disponibilidade` | Fração do tempo com dado válido |
   | `cadencia_fonte` | Intervalo de atualização do feed |
   | `orcamento_de_frescor` | Decomposição da idade da informação por etapa |

Como `dataHora` tem resolução de minuto, `idade_dado_s` tem incerteza de até 60 s.

## 9. Limitações conhecidas

- O rótulo é alagamento registrado, não refluxo comprovado, e os registros subestimam o número de eventos.
- A maré usada é a prevista pela tábua, não a observada, e não inclui efeitos meteorológicos.
- Os limiares iniciais são provisórios; a unidade do campo `chuva` da APAC ainda precisa ser confirmada.
- O feedback anônimo é vulnerável a envenenamento. A simulação mede o dano para dois modelos simples de atacante, não para ataques adaptativos.
- A regra de adaptação foi ajustada com o auxílio do próprio simulador. Antes de afirmar que ela generaliza, valide-a em dados e cenários não usados no seu desenvolvimento.
- O arquivo `data/feedback_log.csv` contém identificadores de usuário e não deve ser publicado.

## 10. Referência de arquivos

| Arquivo | Função |
|---|---|
| `sensor_stats.py` | Métricas, janelas, *bootstrap* e regressão de Firth |
| `experimento.ipynb` | Notebook de análise; inicia em modo sintético |
| `checar_experimento.py` | Checagem das camadas A, B e C |
| `simulador_feedback.py` | Simulação do mecanismo de feedback |
| `mesclar_logs.py` | União dos logs de vários coletores |
| `reconstruir_chuva.py` | Reconstrução do histórico de chuva a partir dos JSON brutos |
| `converter_inmet.py` | Conversão de dados do INMET para o formato de *replay* |
| `frescor.py`, `mqtt_logger.py` | Experimento de frescor |
| `mock_apac.py` | Feed simulado, apenas para testes |
| `gerar_sinteticos.py` | Dados sintéticos, apenas para testes |
| `ocorrencias_template.csv` | Modelo do arquivo de ocorrências |