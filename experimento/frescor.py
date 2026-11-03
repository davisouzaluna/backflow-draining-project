"""Análise de frescor (Age of Information), latência e disponibilidade do pipeline."""
import numpy as np
import pandas as pd


def carregar_recebimentos(caminho):
    r = pd.read_csv(caminho)
    r = r[r["retained"] == 0].drop_duplicates("seq").sort_values("recv_ms")
    return r.reset_index(drop=True)


def aoi(recv_ms, gen_ms):
    """AoI médio e de pico (ms) para entregas (recv) com instante de geração (gen)."""
    r = np.asarray(recv_ms, float)
    g = np.maximum.accumulate(np.asarray(gen_ms, float))
    if len(r) < 2:
        return {"aoi_medio_ms": np.nan, "aoi_pico_medio_ms": np.nan, "aoi_pico_p95_ms": np.nan}
    a0, a1 = r[:-1] - g[:-1], r[1:] - g[:-1]
    area = ((a0 + a1) / 2 * (r[1:] - r[:-1])).sum() / (r[-1] - r[0])
    return {"aoi_medio_ms": area, "aoi_pico_medio_ms": a1.mean(),
            "aoi_pico_p95_ms": np.quantile(a1, 0.95)}


def resumo_frescor(rec):
    gen_sensor = rec["pub_ms"]
    gen_fonte = rec["pub_ms"] - rec["idade_dado_s"] * 1000.0
    return {"rede_sensor_ate_painel": aoi(rec["recv_ms"], gen_sensor),
            "ponta_a_ponta_com_fonte": aoi(rec["recv_ms"], gen_fonte)}


def cadencia_fonte(log):
    """Intervalo entre mudanças de 'chuva_ultima_leitura' observadas no log (cadência da APAC)."""
    ul = pd.to_datetime(log["chuva_ultima_leitura"], errors="coerce").dropna()
    mud = ul[ul != ul.shift()]
    return mud.diff().dt.total_seconds().dropna()


def disponibilidade(log):
    ok_http = (log["http_code"] == 200)
    ok_dado = (log["chuva_ok"] == 1)
    ip = log.index.to_series().diff().dt.total_seconds().dropna()
    return {"leituras": len(log), "pct_http_200": 100 * ok_http.mean(),
            "pct_dado_valido": 100 * ok_dado.mean(),
            "fetch_ms_mediana": log["fetch_ms"].median(), "fetch_ms_p95": log["fetch_ms"].quantile(.95),
            "maior_lacuna_s": ip.max(),
            "idade_dado_s_mediana": log["idade_dado_s"].median(),
            "idade_dado_s_p95": log["idade_dado_s"].quantile(.95)}


def orcamento_de_frescor(log, rec, intervalo_s):
    """Decomposição média da idade da informação entregue ao painel (segundos)."""
    idade_fonte = log["idade_dado_s"].median()
    espera_polling = intervalo_s / 2
    fetch = log["fetch_ms"].median() / 1000
    transp = rec["latencia_ms"].median() / 1000
    return pd.Series({"idade_na_fonte_s (inclui atraso de publicação e cadência)": idade_fonte,
                      "espera_do_polling_s (~intervalo/2)": espera_polling,
                      "busca_http_s": fetch, "mqtt_sensor_ate_painel_s": transp})
