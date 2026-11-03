#!/usr/bin/env python3
"""Inspeciona o feed da APAC/CEMADEN: o que ele devolve e se ha historico.

Rode NA SUA MAQUINA (o sandbox do assistente nao alcanca o host da APAC):
    python3 scripts/inspecionar_apac.py [URL] [--salvar data/raw/amostra.json]

Responde: (1) o formato bate com o que o parser em C espera? (2) quantas
estacoes/cidades? (3) cada estacao traz so a ultima leitura ou uma serie?
(4) qual a cadencia e a idade do dado? (5) o "chuva" e acumulado em que janela?
So usa a biblioteca padrao.
"""
import argparse
import collections
import json
import statistics
import sys
import time
import urllib.request
from datetime import datetime

URL_PADRAO = "http://dados.apac.pe.gov.br:41120/cemaden/"


def baixar(url):
    t0 = time.time()
    with urllib.request.urlopen(url, timeout=30) as r:
        corpo = r.read()
        return r.status, corpo, (time.time() - t0) * 1000


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("url", nargs="?", default=URL_PADRAO)
    ap.add_argument("--salvar")
    ap.add_argument("--cidade", default="RECIFE")
    a = ap.parse_args()

    try:
        status, corpo, ms = baixar(a.url)
    except Exception as e:
        print(f"FALHA ao baixar: {e}")
        sys.exit(2)
    print(f"HTTP {status} | {len(corpo)} bytes | {ms:.0f} ms")
    if a.salvar:
        open(a.salvar, "wb").write(corpo)
        print(f"salvo em {a.salvar}")

    try:
        dados = json.loads(corpo)
    except ValueError as e:
        print(f"Nao e JSON valido: {e}\nInicio: {corpo[:200]!r}")
        sys.exit(2)
    if not isinstance(dados, list):
        print(f"Raiz nao e lista (tipo {type(dados).__name__}); chaves: {list(dados)[:10] if isinstance(dados, dict) else ''}")
        sys.exit(2)
    print(f"itens na raiz: {len(dados)}")

    regs = []
    sem_envelope = 0
    for it in dados:
        bruto = it.get("Dados_completos") if isinstance(it, dict) else None
        if bruto is None:
            sem_envelope += 1
            continue
        try:
            regs.append(json.loads(bruto) if isinstance(bruto, str) else bruto)
        except ValueError:
            sem_envelope += 1
    print(f"registros com envelope 'Dados_completos' valido: {len(regs)} (invalidos: {sem_envelope})")
    if not regs:
        print("O parser em C espera itens com 'Dados_completos'. Ajuste JSON_CAMPO_* em include/config.h.")
        if dados:
            print("Exemplo de item:", json.dumps(dados[0], ensure_ascii=False)[:400])
        sys.exit(1)

    chaves = collections.Counter(k for r in regs for k in r)
    print("campos nos registros:", dict(chaves))
    cidades = collections.Counter(str(r.get("cidade", "?")).upper() for r in regs)
    print("cidades (top 8):", cidades.most_common(8))

    alvo = [r for r in regs if str(r.get("cidade", "")).upper() == a.cidade.upper()]
    print(f"\n== {a.cidade}: {len(alvo)} registros ==")
    if not alvo:
        print("Nenhum registro dessa cidade. Confira CIDADE_ALVO em config.h.")
        sys.exit(1)

    def parse_dh(s):
        s = str(s).split(".")[0]  # o feed manda "2026-10-18 18:00:00.0"
        for fmt in ("%Y-%m-%d %H:%M:%S", "%Y-%m-%d %H:%M", "%Y-%m-%dT%H:%M:%S", "%d/%m/%Y %H:%M"):
            try:
                return datetime.strptime(str(s), fmt)
            except ValueError:
                pass
        return None

    dhs = [parse_dh(r.get("dataHora")) for r in alvo]
    validos = [d for d in dhs if d]
    if not validos:
        print("Nao consegui interpretar 'dataHora'; exemplo:", alvo[0].get("dataHora"))
    else:
        distintos = sorted(set(validos))
        print(f"timestamps distintos: {len(distintos)} | de {distintos[0]} a {distintos[-1]}")
        span_h = (distintos[-1] - distintos[0]).total_seconds() / 3600
        if len(distintos) > 1:
            passos = [(b - a_).total_seconds() / 60 for a_, b in zip(distintos, distintos[1:])]
            print(f"passo entre timestamps: mediana {statistics.median(passos):.1f} min")
        from datetime import timezone
        loc = datetime.now()
        utc = datetime.now(timezone.utc).replace(tzinfo=None)
        i_loc = (loc - distintos[-1]).total_seconds() / 60
        i_utc = (utc - distintos[-1]).total_seconds() / 60
        print(f"idade do dado mais recente: {i_loc:.0f} min se o feed for LOCAL | {i_utc:.0f} min se for UTC")
        if i_loc < -30 and i_utc >= -5:
            print("  => idade negativa no fuso local: o feed esta em UTC. Defina APAC_DATAHORA_UTC 1 em include/config.h.")
        elif i_loc >= -5 and abs(i_loc) < abs(i_utc):
            print("  => o feed parece estar no horario local (APAC_DATAHORA_UTC 0).")
        else:
            print("  => inconclusivo; compare a hora do dado com a hora real de uma chuva conhecida.")
        por_estacao = collections.Counter(json.dumps({k: r.get(k) for k in ("codestacao", "estacao", "nome") if k in r}, sort_keys=True) for r in alvo)
        n_est = len(por_estacao)
        print(f"estacoes distintas (por campos de identificacao): {n_est}")
        print("\nVEREDITO SOBRE HISTORICO:")
        ts_por_est = collections.defaultdict(set)
        for r, d in zip(alvo, dhs):
            if d:
                ts_por_est[r.get("codestacao", r.get("nome", "?"))].add(d)
        med_ts = statistics.median(len(v) for v in ts_por_est.values())
        print(f"instantes distintos por estacao: mediana {med_ts:.0f}")
        if med_ts > 1:
            print(f"  O feed traz uma SERIE de ~{span_h:.1f} h. Coletas esparsas (a cada poucas horas) "
                  "permitem reconstruir o periodo entre elas: guarde o JSON bruto (data/raw/) em cada coleta.")
        else:
            print("  O feed parece trazer so a ULTIMA leitura por estacao. Buracos na coleta sao "
                  "irrecuperaveis: o que nao for coletado nao existe. Planeje redundancia (VM + PC + coleta em nuvem).")

    # chuva: por estacao e por instante, para descobrir a janela do acumulado
    por_cod = collections.defaultdict(list)
    for r, d in zip(alvo, dhs):
        if d and r.get("chuva") not in (None, ""):
            try:
                por_cod[r.get("codestacao", "?")].append((d, float(str(r["chuva"]).replace(",", "."))))
            except ValueError:
                pass
    todos = [v for lst in por_cod.values() for _, v in lst]
    if todos:
        print(f"\nchuva: {len(todos)} leituras de {len(por_cod)} estacoes | min={min(todos)} "
              f"mediana={statistics.median(todos)} max={max(todos)} | leituras > 0: {sum(v > 0 for v in todos)}")
        qtd = sorted(len(l) for l in por_cod.values())
        print(f"leituras por estacao: min={qtd[0]} mediana={qtd[len(qtd)//2]} max={qtd[-1]}")
        ult = max(d for lst in por_cod.values() for d, _ in lst)
        nl = [v for lst in por_cod.values() for d, v in lst if d == ult]
        print(f"no instante mais recente ({ult}): {len(nl)} estacoes com chuva, media={sum(nl)/len(nl):.2f} mm")
        print("  O sensor em C faz a media sobre TODOS os registros de Recife do feed, nao so o instante mais recente.")
        print("  Se houver serie por estacao, isso mistura horas diferentes. Confirme acima e ajuste chuva_agregar.")
        print("  Janela do acumulado: so se descobre com dia de chuva (compare duas leituras seguidas) ou na documentacao.")
    else:
        print("\nchuva: nenhum valor numerico em", a.cidade)
    print("\nExemplo de registro:", json.dumps(alvo[0], ensure_ascii=False))


if __name__ == "__main__":
    main()