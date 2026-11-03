# Protocolo: coletar, validar e checar o sensor virtual (Recife)

## 0. O que existe e o que NÃO existe (leia primeiro)
- **Existe e foi testado (sandbox):** compilação; testes unitários; sensor lendo um *mock* da APAC; MQTT com broker local; feedback de usuários alterando limiares; `--once`; análise estatística em dados **sintéticos**; simulador de feedback; checador do experimento (detecta 4 falhas injetadas).
- **NÃO foi testado:** o feed **real** da APAC (o sandbox não alcança o host). O formato real, o fuso, a janela do acumulado ("chuva" é 10 min? 1 h? 24 h?) e se o feed traz histórico são **desconhecidos**. Comece por `python3 scripts/inspecionar_apac.py` na sua máquina.
- **Não existe ainda:** ocorrências reais de alagamento (`experimento/ocorrencias.csv`). Sem elas não há validação de acurácia, só de qualidade da coleta e do mecanismo.
- Todos os números do simulador e do notebook em modo `sintetico` valem **só para testar o código**. Nunca vão para o artigo como resultado.

## 1. Primeiro dia (30 min)
1. `python3 scripts/inspecionar_apac.py --salvar data/raw/amostra.json` → leia o "VEREDITO SOBRE HISTÓRICO".
   - Se o feed traz **série** (várias horas): coletas esparsas ainda permitem reconstruir o período entre elas; guarde o JSON bruto (`data/raw/`).
   - Se traz **só a última leitura**: buraco de coleta é perda definitiva → use redundância (item 2).
2. Confirme a unidade/janela do campo `chuva` e o fuso de `dataHora` (se `idade_dado_s` vier com deslocamento de horas, ajuste `APAC_DATAHORA_UTC`).
3. `cmake -S . -B build && cmake --build build && ctest --test-dir build`.

## 2. Como executar quando o PC não fica ligado e a VM pode cair
Princípio: **a coleta não pode depender de uma máquina só**, e toda interrupção precisa ser *visível* nos dados.
1. **VM da universidade (principal):** `deploy/sensor-virtual.service` (systemd, `Restart=always`). Volta sozinho após queda/reboot; log e estado de limiares são gravados de forma atômica; `boot_id`/`seq` no log mostram cada reinício e salto.
2. **Seu PC (secundário):** `deploy/coletar_once.sh` no cron (`*/10 * * * *`) ou o laço normal enquanto estiver ligado. Em outra pasta `data/`, para não misturar estado de limiares.
3. **Nuvem (opcional, NÃO testado):** `.github/workflows/coletar.yml` coleta a cada 15 min no GitHub Actions (atrasa e pode pular; precisa alcançar a porta 41120). Use só como complemento.
4. **Unir:** `python mesclar_logs.py vm.csv pc.csv -o ../data/leituras_mescladas.csv --intervalo 600`. Em teste sintético, dois coletores com ~89% de cobertura cada, falhando em horários independentes, deram ~98% unidos.
5. Os campos de risco/alerta dos coletores secundários usam o estado de limiares **deles**. Na análise use sempre chuva e maré brutas e recalcule o risco (`risco_offline`) com os limiares do coletor principal.
6. Agende `checar_experimento.py` diariamente (item 3) para saber cedo se algo parou.

## 3. Como saber se o experimento faz sentido (3 camadas)
Rode: `python checar_experimento.py --log ../data/leituras_log.csv --tabua ../data/mare_recife.csv [--ocorrencias ocorrencias.csv] [--feedback ../data/feedback_log.csv]` (relatório em `resultados/checagem.md`).

**Camada A – a coleta é utilizável?** cobertura temporal; lacunas e reinícios; cobertura por hora do dia; **cobertura × fase da maré e × chuva** (se o PC some justamente quando chove ou na maré alta, a amostra é enviesada); disponibilidade do feed; idade do dado; plausibilidade física.

**Camada B – a implementação está correta?** maré do C == maré recomputada em Python; `alerta` registrado == regra recalculada com os limiares do log; nenhum alerta sem chuva (controle negativo); fração do tempo com maré ≥ limiar (a versão antiga dava ~50%). Além disso: `ctest` (convergência, limites, interpolação) e `simulador_feedback.py --paridade build/replay_passo` (regra do C == regra do Python, diferença < 1e-9).

**Camada C – o sensor acerta mais que o acaso?** (precisa de ocorrências) cobertura nos momentos das ocorrências; **placebo por deslocamento temporal** (CSI observado vs ocorrências deslocadas, preserva autocorrelação); baselines só-chuva e só-maré; interação chuva×maré.

## 4. Validação estatística (plano a congelar antes de olhar o teste)
- **Unidade:** janela de 1 h (sensibilidade: 30 min e 3 h; antecedência 0, 1, 3 h). Rótulo = houve ocorrência na janela.
- **Divisão temporal:** primeiros ~60% para calibrar, restante para teste com limiares congelados (ou rolling-origin). Se o feedback estiver ligado, **congele os limiares** (`ADAPT_HABILITADO 0`) no período de teste, senão o alvo se move.
- **Desfecho principal:** TSS no teste, IC95 por bootstrap em blocos de 24 janelas. Secundários: POD, FAR, CSI, MCC, antecedência, desempenho por evento.
- **Hipótese central:** chuva e maré juntas superam cada uma isolada (interação por Firth+LRT, ΔAUC por bootstrap em blocos, McNemar exato pareado, Holm).
- **Tamanho amostral:** ~10 eventos por preditor (30 para 3); ~60 eventos para IC95 de largura 0,20 em POD=0,8. Com menos, reporte regra simples com IC exatos e estudos de caso.
- **Feedback (simulação):** `python simulador_feedback.py --seeds 30 --paridade ../build/replay_passo`. Compara política estática, só-confirmação-de-alertas, completa (exploração + relatos espontâneos) e oráculo, em 10 cenários (limiar inicial alto/baixo, deriva, poucos/muitos usuários, ruído, envenenamento aleatório e dirigido). Teste pareado de Wilcoxon por semente + Holm. Isso valida o **mecanismo**; não prova utilidade com usuários reais.
- **Feedback (real):** piloto com usuários exige avaliação ética (CEP/CNS 510/2016) e ≥ dezenas de respostas por semana para o mecanismo ter efeito (o simulador mostra efeito nulo com ~0,1 resposta/pergunta).

## 5. Limitações a declarar
- Rótulo = alagamento registrado, não refluxo comprovado; registros subestimam eventos.
- Maré = previsão da tábua, não observada (sem surgência meteorológica).
- Limiares iniciais provisórios; unidade da chuva da APAC a confirmar.
- Feedback anônimo é vulnerável a envenenamento; o simulador mede o dano em dois modelos simples de atacante, não em ataques adaptativos.
- A regra de adaptação foi projetada com a ajuda do próprio simulador (a versão inicial derivava sob ruído); por isso valide-a em **dados/cenários não usados no projeto** antes de afirmar generalização.

## 6. Arquivos
`sensor_stats.py` (métricas, janelas, bootstrap, Firth) · `experimento.ipynb` (análise; começa em modo sintético) · `checar_experimento.py` · `simulador_feedback.py` · `mesclar_logs.py` · `frescor.py` + `mqtt_logger.py` (experimento de frescor) · `mock_apac.py` (só teste) · `gerar_sinteticos.py` (só teste) · `ocorrencias_template.csv`.

## 7. Experimento de frescor (independe de ocorrências)
1. Em paralelo ao sensor, rode `python mqtt_logger.py --saida recebimentos.csv` (relógios sincronizados por NTP ou mesma máquina).
2. Analise com `frescor.py`: `resumo_frescor` (AoI médio e de pico), `disponibilidade`, `cadencia_fonte`, `orcamento_de_frescor`.
3. `dataHora` tem resolução de minuto → `idade_dado_s` tem incerteza de até 60 s; declare.
