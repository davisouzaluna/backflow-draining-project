"""Simulador do laco de feedback (MAPE-K) do sensor virtual.

Reusa a regra de atualizacao do codigo C (adaptador_passo) e simula usuarios
que respondem a perguntas ou relatam alagamentos. O "mundo" e sintetico: ha
limiares verdadeiros (ocultos) e ruido ambiental. Isso mede se o MECANISMO
converge e quanto resiste a erro/envenenamento; NAO mede a utilidade no mundo
real, que depende de usuarios reais.
"""
import argparse
import os
import subprocess
from dataclasses import dataclass, replace

import numpy as np
import pandas as pd
from scipy import stats

import sensor_stats as ss

# constantes espelham include/config.h
R_MIN, R_MAX = 0.0, 50.0
H_MIN, H_MAX = 1.0, 2.7
TAXA, PESO_PERDA, DELTA, RHO = 0.3, 2.0, 0.02, 0.15
LIM_INICIAL = (5.0, 2.0)


def passo(lim, r, h, alerta, houve, peso=1.0):
    """Copia fiel de adaptador_passo (C). Perceptron com margem e regiao de confianca."""
    lr, lh = lim
    eta = min(max(TAXA * peso, 0.0), 1.0)
    sr, sh = R_MAX - R_MIN, H_MAX - H_MIN
    gr, gh = (r - lr) / sr, (h - lh) / sh
    s_and = min(gr, gh)
    if houve:
        if s_and < DELTA:
            falta_r, falta_h = max(DELTA - gr, 0.0), max(DELTA - gh, 0.0)
            if falta_r <= RHO and falta_h <= RHO:
                e = min(max(eta * (PESO_PERDA if s_and < 0 else 1.0), 0.0), 1.0)
                lr -= e * falta_r * sr
                lh -= e * falta_h * sh
    elif s_and > -DELTA:
        if gr <= gh:
            lr += eta * (gr + DELTA) * sr
        else:
            lh += eta * (gh + DELTA) * sh
    return (min(max(lr, R_MIN), R_MAX), min(max(lh, H_MIN), H_MAX))


@dataclass
class Cenario:
    nome: str = "estacionario"
    lim_verdadeiro: tuple = (None, None)      # None -> definido pelos dados
    deriva_para: tuple = None                 # limiares verdadeiros apos a metade
    p_alag_regra: float = 0.9                 # P(alagar | regra verdadeira)
    p_alag_base: float = 0.005                # P(alagar | fora da regra)
    lam_resp: float = 0.5                     # respondentes por pergunta (Poisson)
    acuracia: float = 0.85                    # P(resposta correta)
    frac_maliciosa: float = 0.0               # fracao de respondentes maliciosos
    modo_malicioso: str = "inverte"           # inverte | sempre_nao (suprime alertas)
    lam_espontaneo: float = 0.3               # relatos por alagamento sem alerta
    max_resp_passo: int = 3                   # limite de respostas aceitas por passo
    eps: float = 0.02                         # exploracao
    inicio: str = "alto"                      # calibrado | alto | baixo (relativo ao verdadeiro)


POLITICAS = ("estatica", "so_alertas", "completa", "oraculo")


ESCALA_CHUVA = 4.0   # os sinteticos tem chuva fraca; reescala so para a simulacao


def contextos_horarios(log, escala=None):
    escala = ESCALA_CHUVA if escala is None else escala
    h = log.resample("1h").agg(r=("chuva_media_mm", "max"), h=("mare_interp_m", "max")).dropna()
    return h["r"].to_numpy(float) * escala, h["h"].to_numpy(float)


def lim_inicial(cen):
    r, h = cen.lim_verdadeiro
    if cen.inicio == "calibrado":
        return (r, h)
    if cen.inicio == "baixo":
        return (max(r * 0.4, 0.5), max(h - 0.3, H_MIN))
    return (min(r * 2.0, R_MAX), min(h + 0.3, H_MAX))


def definir_limiares_verdadeiros(r, h, alvo_eventos_por_mil=8.0):
    """Escolhe (R*, H*) com maré em ~2.1 m e chuva tal que a regra E dispare em ~0,8% das horas."""
    h_star = 2.1
    cand = np.quantile(r, np.linspace(0.5, 0.999, 300))
    taxa = np.array([np.mean((r >= c) & (h >= h_star)) * 1000 for c in cand])
    c = cand[np.argmin(np.abs(taxa - alvo_eventos_por_mil))]
    return (float(round(c, 1)), h_star)


def rodar(r, h, pol, cen, seed):
    rng = np.random.default_rng(seed)
    n = len(r)
    desloc = int(rng.integers(0, n))
    r, h = np.roll(r, desloc), np.roll(h, desloc)
    lim = lim_inicial(cen)
    verd = cen.lim_verdadeiro
    meio = n // 2
    alertas = np.zeros(n, bool)
    verdade = np.zeros(n, bool)
    lim_hist = np.zeros((n, 2))
    atualizacoes = 0
    for t in range(n):
        v = cen.deriva_para if (cen.deriva_para and t >= meio) else verd
        regra = r[t] >= v[0] and h[t] >= v[1]
        y = rng.random() < (cen.p_alag_regra if regra else cen.p_alag_base)
        if pol == "oraculo":
            lim = v
        alerta = (r[t] >= lim[0]) and (h[t] >= lim[1])
        alertas[t], verdade[t], lim_hist[t] = alerta, y, lim
        if pol in ("estatica", "oraculo"):
            continue
        respostas = []
        pergunta = alerta or (pol == "completa" and rng.random() < cen.eps)
        if pergunta:
            for _ in range(rng.poisson(cen.lam_resp)):
                if rng.random() < cen.frac_maliciosa:
                    respostas.append(False if cen.modo_malicioso == "sempre_nao" else (not y))
                else:
                    respostas.append(y if rng.random() < cen.acuracia else not y)
        if pol == "completa" and y and not alerta:
            for _ in range(rng.poisson(cen.lam_espontaneo)):
                if rng.random() < cen.frac_maliciosa:
                    continue
                respostas.append(True)
        for houve in respostas[: cen.max_resp_passo]:
            novo = passo(lim, r[t], h[t], alerta, bool(houve))
            atualizacoes += novo != lim
            lim = novo
    return alertas, verdade, lim_hist, atualizacoes, (cen.deriva_para or verd)


def avaliar(alertas, verdade, lim_hist, v_final, metade="segunda"):
    n = len(alertas)
    sl = slice(n // 2, n) if metade == "segunda" else slice(0, n)
    m = ss.metricas(verdade[sl], alertas[sl])
    erro_r = abs(lim_hist[-1, 0] - v_final[0]) / (R_MAX - R_MIN)
    erro_h = abs(lim_hist[-1, 1] - v_final[1]) / (H_MAX - H_MIN)
    return {**{k: m[k] for k in ("POD", "FAR", "CSI", "TSS", "MCC")},
            "erro_limiar": float(np.hypot(erro_r, erro_h)),
            "eventos": int(verdade[sl].sum())}


def experimento(log, cenarios, politicas=POLITICAS, seeds=30):
    r, h = contextos_horarios(log)
    linhas = []
    for cen in cenarios:
        if cen.lim_verdadeiro[0] is None:
            cen = replace(cen, lim_verdadeiro=definir_limiares_verdadeiros(r, h))
        for pol in politicas:
            for s in range(seeds):
                a, v, lh, upd, vf = rodar(r, h, pol, cen, seed=1000 * s + 17)
                linhas.append({"cenario": cen.nome, "politica": pol, "seed": s,
                               "atualizacoes": upd, "R_verd": cen.lim_verdadeiro[0],
                               **avaliar(a, v, lh, vf)})
    return pd.DataFrame(linhas)


def ic_boot(x, B=4000, seed=0):
    rng = np.random.default_rng(seed)
    x = np.asarray(x, float)
    m = rng.choice(x, (B, len(x))).mean(1)
    return np.nanpercentile(m, [2.5, 97.5])


def resumir(df):
    linhas = []
    for (c, p), g in df.groupby(["cenario", "politica"]):
        d = {"cenario": c, "politica": p, "n_seeds": len(g)}
        for k in ("POD", "FAR", "CSI", "TSS", "erro_limiar"):
            lo, hi = ic_boot(g[k].fillna(0))
            d[k] = g[k].mean()
            d[k + "_ic"] = f"[{lo:.3f}, {hi:.3f}]"
        linhas.append(d)
    return pd.DataFrame(linhas)


def testes_pareados(df, base="estatica", alvo="completa", metrica="CSI"):
    saida = []
    for c, g in df.groupby("cenario"):
        a = g[g.politica == alvo].sort_values("seed")[metrica].fillna(0).to_numpy()
        b = g[g.politica == base].sort_values("seed")[metrica].fillna(0).to_numpy()
        if np.allclose(a, b):
            p = 1.0
        else:
            p = stats.wilcoxon(a, b).pvalue
        saida.append({"cenario": c, "metrica": metrica, "dif_media": float((a - b).mean()),
                      "frac_seeds_melhor": float((a > b).mean()), "p_wilcoxon": p})
    t = pd.DataFrame(saida)
    t["p_holm"] = ss.holm(t["p_wilcoxon"].to_numpy())
    return t


def cenarios_padrao():
    return [
        Cenario("calibrado", inicio="calibrado"),
        Cenario("inicio_alto"),
        Cenario("inicio_baixo", inicio="baixo"),
        Cenario("deriva", inicio="calibrado", deriva_para=(None, 1.8)),
        Cenario("alto_poucos_usuarios", lam_resp=0.1, lam_espontaneo=0.05),
        Cenario("alto_muitos_usuarios", lam_resp=2.0, lam_espontaneo=1.0),
        Cenario("alto_ruido_30pct", acuracia=0.70),
        Cenario("calibrado_veneno_10pct", inicio="calibrado", frac_maliciosa=0.10),
        Cenario("calibrado_veneno_30pct", inicio="calibrado", frac_maliciosa=0.30),
        Cenario("calibrado_veneno_nao_30pct", inicio="calibrado", frac_maliciosa=0.30,
                modo_malicioso="sempre_nao"),
    ]


def verificar_paridade(binario, n=2000, seed=3):
    rng = np.random.default_rng(seed)
    casos = []
    for _ in range(n):
        casos.append((rng.uniform(R_MIN, R_MAX), rng.uniform(H_MIN, H_MAX),
                      rng.uniform(0, 30), rng.uniform(0.5, 3.0),
                      int(rng.integers(0, 2)), int(rng.integers(0, 2)),
                      float(rng.choice([0.0, 0.5, 1.0, 2.0]))))
    entrada = "\n".join(" ".join(repr(x) for x in c) for c in casos) + "\n"
    saida = subprocess.run([binario], input=entrada, capture_output=True, text=True,
                           check=True).stdout.split("\n")
    maxdif = 0.0
    for c, linha in zip(casos, saida):
        cr, ch = map(float, linha.split())
        pr, ph = passo((c[0], c[1]), c[2], c[3], c[4], c[5], c[6])
        maxdif = max(maxdif, abs(cr - pr), abs(ch - ph))
    return maxdif


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--log", default="sintetico/sintetico_log.csv")
    ap.add_argument("--seeds", type=int, default=30)
    ap.add_argument("--saida", default="resultados")
    ap.add_argument("--paridade", help="caminho de build/replay_passo")
    a = ap.parse_args()
    os.makedirs(a.saida, exist_ok=True)
    if a.paridade:
        d = verificar_paridade(a.paridade)
        print(f"paridade C x Python: maior diferenca = {d:.2e}")
        assert d < 1e-9, "regra do Python difere da do C"
    log = ss.carregar_log(a.log)
    r, h = contextos_horarios(log)
    vstar = definir_limiares_verdadeiros(r, h)
    cen = []
    for c in cenarios_padrao():
        if c.deriva_para is not None:
            c = replace(c, deriva_para=(vstar[0] * 0.6, c.deriva_para[1]))
        cen.append(c)
    df = experimento(log, cen, seeds=a.seeds)
    df.to_csv(f"{a.saida}/simulacao_bruta.csv", index=False)
    res = resumir(df)
    res.to_csv(f"{a.saida}/simulacao_resumo.csv", index=False)
    print(f"limiares verdadeiros (definidos pelos dados): chuva={vstar[0]} mm, mare={vstar[1]} m")
    print(res[["cenario", "politica", "CSI", "CSI_ic", "TSS", "erro_limiar"]].round(3).to_string(index=False))
    for met in ("CSI", "TSS"):
        t = testes_pareados(df, "estatica", "completa", met)
        t.to_csv(f"{a.saida}/simulacao_testes_{met}.csv", index=False)
        print(f"\ncompleta vs estatica ({met}), Wilcoxon pareado + Holm:")
        print(t.round(4).to_string(index=False))
    t = testes_pareados(df, "so_alertas", "completa", "TSS")
    print("\ncompleta vs so_alertas (TSS):")
    print(t.round(4).to_string(index=False))


if __name__ == "__main__":
    main()
