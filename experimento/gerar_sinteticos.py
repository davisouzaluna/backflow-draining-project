"""Gera DADOS SINTÉTICOS para testar o pipeline. Não usar no artigo."""
import os

import numpy as np
import pandas as pd

import sensor_stats as ss

AQUI = os.path.dirname(os.path.abspath(__file__))
SAIDA = os.path.join(AQUI, "sintetico")
TABUA = os.path.join(AQUI, "..", "data", "mare_recife.csv")


def gerar(inicio="2026-03-01", fim="2026-07-31", passo_min=5, semente=7,
          b=(-9.6, 1.4, 0.8, 1.2), lacunas=0):
    rng = np.random.default_rng(semente)
    tabua = ss.carregar_tabua(TABUA)
    idx = pd.date_range(inicio, fim, freq=f"{passo_min}min", tz=ss.TZ)
    n = len(idx)

    chuva = np.zeros(n)
    passos_h = 60 // passo_min
    for _ in range(rng.poisson(0.55 * len(idx) / (24 * passos_h))):
        i = rng.integers(0, n)
        dur = rng.integers(1, 9) * passos_h
        pico = rng.lognormal(1.2, 0.7)
        forma = np.sin(np.linspace(0, np.pi, dur))
        fim_i = min(n, i + dur)
        chuva[i:fim_i] += pico * forma[: fim_i - i] * rng.uniform(0.6, 1.2, fim_i - i)
    chuva = np.round(chuva, 2)

    h_real = ss.mare_interpolada(tabua, idx)
    te = ss._ns(tabua["instante"]) / 1e9
    t = ss._ns(idx) / 1e9
    j = np.clip(np.searchsorted(te, t), 1, len(te) - 1)
    mais_prox = np.where(np.abs(te[j] - t) < np.abs(te[j - 1] - t), j, j - 1)
    ev = tabua.iloc[mais_prox].reset_index(drop=True)
    dia = ev["instante"].dt.date
    mn = tabua.groupby(tabua["instante"].dt.date)["altura"].min()
    mx = tabua.groupby(tabua["instante"].dt.date)["altura"].max()
    lo, hi = dia.map(mn).to_numpy(float), dia.map(mx).to_numpy(float)

    log = pd.DataFrame({
        "epoch": (t).astype(int),
        "boot_id": (t // (86400 * 2)).astype(int) * 86400 * 2,
        "seq": (t % (86400 * 2) // (passo_min * 60)).astype(int) + 1,
        "timestamp": idx.strftime("%Y-%m-%d %H:%M:%S"),
        "chuva_ok": 1,
        "chuva_media_mm": chuva,
        "chuva_maior_mm": np.round(chuva * rng.uniform(1.0, 2.2, n), 2),
        "estacoes": 5,
        "chuva_ultima_leitura": idx.strftime("%Y-%m-%d %H:%M"),
        "mare_ok": 1,
        "mare_evento_data": ev["instante"].dt.strftime("%Y-%m-%d"),
        "mare_evento_hora": ev["instante"].dt.strftime("%H:%M"),
        "mare_evento_m": ev["altura"].to_numpy(),
        "mare_interp_m": np.round(h_real.to_numpy(), 4),
        "mare_min_dia_m": lo,
        "mare_max_dia_m": hi,
        "mare_dif_min": np.round(np.abs(te[mais_prox] - t) / 60).astype(int),
    })
    risco = ss.risco_offline(log["chuva_media_mm"], log["mare_interp_m"])
    log["limiar_chuva_mm"] = 5.0
    log["limiar_mare_m"] = 2.0
    log["alerta"] = (risco == 3).astype(int)
    log["pergunta"] = 0
    log["risco_codigo"] = risco
    log["risco_nome"] = pd.Series(risco).map(
        {0: "DADOS INSUFICIENTES", 1: "SEM RISCO", 2: "RISCO DE BACKFLOW",
         3: "BACKFLOW EM OCORRENCIA"})

    h = pd.DataFrame({"c": np.log1p(chuva), "m": h_real.to_numpy()}, index=idx)
    hh = h.resample("1h").agg({"c": "max", "m": "max"}).dropna()
    zc, zm = (hh["c"] - hh["c"].mean()) / hh["c"].std(), (hh["m"] - hh["m"].mean()) / hh["m"].std()
    eta = b[0] + b[1] * zc + b[2] * zm + b[3] * zc * zm
    p = 1 / (1 + np.exp(-eta))
    ocorre = (rng.random(len(hh)) < p) & (rng.random(len(hh)) < 0.75)
    ini = hh.index[ocorre] + pd.to_timedelta(rng.integers(0, 3600, ocorre.sum()), unit="s")
    oc = pd.DataFrame({"id": range(1, len(ini) + 1),
                       "inicio": ini.tz_localize(None).strftime("%Y-%m-%d %H:%M"),
                       "fim": (ini + pd.Timedelta("90min")).tz_localize(None)
                       .strftime("%Y-%m-%d %H:%M"),
                       "bairro": "SINTETICO", "lat": "", "lon": "", "fonte": "sintetico",
                       "tipo": "alagamento", "confianca": "alta", "observacao": ""})

    log["http_code"] = 200
    log["curl_rc"] = 0
    log["fetch_ms"] = np.round(rng.lognormal(5.5, 0.4, n), 1)
    log["idade_dado_s"] = np.round(rng.uniform(60, 660, n), 1)
    if lacunas:
        manter = np.ones(n, bool)
        for _ in range(lacunas):
            i = rng.integers(0, n)
            manter[i:i + int(rng.integers(6, 30) * 60 / passo_min)] = False
        log = log[manter].reset_index(drop=True)

    os.makedirs(SAIDA, exist_ok=True)
    log.to_csv(os.path.join(SAIDA, "sintetico_log.csv"), index=False)
    oc.to_csv(os.path.join(SAIDA, "sintetico_ocorrencias.csv"), index=False)
    return log, oc


if __name__ == "__main__":
    lg, oc = gerar()
    print(f"log: {len(lg)} leituras | ocorrencias: {len(oc)}")
