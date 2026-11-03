import io

import numpy as np
import pandas as pd
from scipy import stats
from sklearn.metrics import average_precision_score, roc_auc_score

TZ = "America/Recife"
AJUSTE_DATUM_IBGE_M = -1.14


def _ns(x):
    x = pd.DatetimeIndex(x)
    if x.tz is not None:
        x = x.tz_convert("UTC")
    return x.as_unit("ns").asi8


# ---------- carga de dados ----------

def carregar_log(caminho):
    df = pd.read_csv(caminho)
    df["t"] = pd.to_datetime(df["epoch"], unit="s", utc=True).dt.tz_convert(TZ)
    df = df.drop_duplicates(["epoch"]).sort_values("t").set_index("t")
    df.loc[df["chuva_ok"] == 0, ["chuva_media_mm", "chuva_maior_mm"]] = np.nan
    if "mare_interp_m" not in df:
        df["mare_interp_m"] = df.get("mare_altura_m", np.nan)
    df.loc[df["mare_ok"] == 0, "mare_interp_m"] = np.nan
    return df


def carregar_ocorrencias(caminho):
    o = pd.read_csv(caminho, comment="#")
    o["inicio"] = pd.to_datetime(o["inicio"]).dt.tz_localize(TZ)
    if "fim" in o:
        o["fim"] = pd.to_datetime(o["fim"]).dt.tz_localize(TZ)
    return o.sort_values("inicio").reset_index(drop=True)


def carregar_tabua(caminho):
    txt = open(caminho, encoding="utf-8").read().replace("\t", ",")
    t = pd.read_csv(io.StringIO(txt), names=["data", "hora", "altura"], header=0,
                    on_bad_lines="skip")
    t["instante"] = pd.to_datetime(t["data"] + " " + t["hora"], errors="coerce")
    t["altura"] = pd.to_numeric(t["altura"], errors="coerce")
    t = t.dropna(subset=["instante", "altura"]).drop_duplicates("instante")
    t["instante"] = t["instante"].dt.tz_localize(TZ)
    return t.sort_values("instante").reset_index(drop=True)


def relatorio_qualidade(df, intervalo_s=30):
    dt = df.index.to_series().diff().dt.total_seconds()
    return {
        "leituras": len(df),
        "inicio": df.index.min(),
        "fim": df.index.max(),
        "lacunas_maiores_que_5x_intervalo": int((dt > 5 * intervalo_s).sum()),
        "maior_lacuna_h": round(float(dt.max()) / 3600, 2),
        "pct_chuva_disponivel": round(float(df["chuva_ok"].mean()) * 100, 1),
        "pct_mare_disponivel": round(float(df["mare_ok"].mean()) * 100, 1),
    }


# ---------- maré offline (alternativas à "evento mais próximo") ----------

def mare_interpolada(tabua, idx, datum_ibge=False):
    te = _ns(tabua["instante"]) / 1e9
    he = tabua["altura"].to_numpy()
    t = _ns(idx) / 1e9
    j = np.searchsorted(te, t) - 1
    ok = (j >= 0) & (j < len(te) - 1)
    jj = np.clip(j, 0, len(te) - 2)
    frac = (t - te[jj]) / (te[jj + 1] - te[jj])
    h = he[jj] + (he[jj + 1] - he[jj]) * (1 - np.cos(np.pi * frac)) / 2
    h = np.where(ok, h, np.nan)
    if datum_ibge:
        h = h + AJUSTE_DATUM_IBGE_M
    return pd.Series(h, index=idx)


def adicionar_mare_interpolada(df, tabua):
    df = df.copy()
    df["mare_interp_m"] = mare_interpolada(tabua, df.index)
    return df


# ---------- regra do sensor refeita offline (permite testar outras metas) ----------

def risco_offline(chuva, mare_m, limiar_chuva=5.0, limiar_mare=2.0, margem=0.7):
    chuva, mare = np.asarray(chuva, float), np.asarray(mare_m, float)
    sem = np.isnan(chuva) | np.isnan(mare)
    ocorrendo = (chuva >= limiar_chuva) & (mare >= limiar_mare)
    potencial = (chuva >= margem * limiar_chuva) & (mare >= margem * limiar_mare)
    r = np.where(ocorrendo, 3, np.where(potencial, 2, 1))
    return np.where(sem, 0, r)


# ---------- janelas e rótulos ----------

def construir_janelas(df, oc, janela="1h", lag="0h", intervalo_s=30, min_cobertura=0.5,
                      col_mare="mare_interp_m"):
    w = df.resample(janela).agg(
        n=("epoch", "size"),
        chuva_max=("chuva_media_mm", "max"),
        chuva_pico_estacao=("chuva_maior_mm", "max"),
        mare_h=(col_mare, "max"),
        risco=("risco_codigo", "max"),
    )
    esperado = pd.Timedelta(janela).total_seconds() / intervalo_s
    w = w[w["n"] >= min_cobertura * esperado].copy()
    w["chuva_ant"] = w["chuva_max"].shift(1).rolling(24, min_periods=6).mean()

    ini = np.sort(_ns(oc["inicio"]))
    lo = _ns(w.index) + pd.Timedelta(lag).value
    hi = lo + pd.Timedelta(janela).value
    w["y"] = (np.searchsorted(ini, hi) - np.searchsorted(ini, lo)) > 0
    return w


def yhat_sensor(w, nivel=2):
    return (w["risco"] >= nivel).to_numpy()


# ---------- métricas ----------

def conf(y, p):
    y, p = np.asarray(y, bool), np.asarray(p, bool)
    return int((y & p).sum()), int((~y & p).sum()), int((y & ~p).sum()), int((~y & ~p).sum())


def metricas(y, p):
    tp, fp, fn, tn = conf(y, p)
    d = lambda a, b: a / b if b else np.nan
    pod, esp, prec = d(tp, tp + fn), d(tn, tn + fp), d(tp, tp + fp)
    den = (tp + fn) * (fn + tn) + (tp + fp) * (fp + tn)
    raiz = np.sqrt(float((tp + fp) * (tp + fn) * (tn + fp) * (tn + fn)))
    return {"TP": tp, "FP": fp, "FN": fn, "TN": tn, "POD": pod,
            "FAR": d(fp, tp + fp), "CSI": d(tp, tp + fp + fn), "ESP": esp,
            "PREC": prec, "TSS": pod + esp - 1, "HSS": d(2 * (tp * tn - fp * fn), den),
            "MCC": d(tp * tn - fp * fn, raiz), "BA": (pod + esp) / 2}


def wilson(k, n, a=0.05):
    if n == 0:
        return (np.nan, np.nan)
    z, p = stats.norm.ppf(1 - a / 2), k / n
    den = 1 + z * z / n
    c = (p + z * z / (2 * n)) / den
    h = z * np.sqrt(p * (1 - p) / n + z * z / (4 * n * n)) / den
    return (c - h, c + h)


def clopper_pearson(k, n, a=0.05):
    lo = stats.beta.ppf(a / 2, k, n - k + 1) if k > 0 else 0.0
    hi = stats.beta.ppf(1 - a / 2, k + 1, n - k) if k < n else 1.0
    return (lo, hi)


def _indices_bloco(n, bloco, B, rng):
    bloco = max(1, min(bloco, n))
    nb = int(np.ceil(n / bloco))
    ini = rng.integers(0, n - bloco + 1, size=(B, nb))
    return (ini[:, :, None] + np.arange(bloco)).reshape(B, -1)[:, :n]


def bootstrap_metricas(y, p, bloco=24, B=2000, seed=0,
                       nomes=("POD", "FAR", "CSI", "TSS", "HSS", "MCC")):
    y, p = np.asarray(y, bool), np.asarray(p, bool)
    idx = _indices_bloco(len(y), bloco, B, np.random.default_rng(seed))
    vals = pd.DataFrame([metricas(y[i], p[i]) for i in idx])[list(nomes)]
    base = metricas(y, p)
    lo, hi = vals.quantile(0.025), vals.quantile(0.975)
    return pd.DataFrame({"estimativa": [base[n] for n in nomes], "ic95_inf": lo.values,
                         "ic95_sup": hi.values}, index=list(nomes))


def tabela_metricas(y, p, bloco=24, B=2000):
    t = bootstrap_metricas(y, p, bloco, B)
    tp, fp, fn, tn = conf(y, p)
    t.loc["POD", ["ic95_inf", "ic95_sup"]] = clopper_pearson(tp, tp + fn)
    return t, {"TP": tp, "FP": fp, "FN": fn, "TN": tn}


# ---------- curvas e comparação pareada ----------

def auc_roc_pr(y, score):
    y, s = np.asarray(y, bool), np.nan_to_num(np.asarray(score, float))
    if y.sum() == 0 or y.all():
        return np.nan, np.nan
    return roc_auc_score(y, s), average_precision_score(y, s)


def auc_diferenca_bloco(y, s1, s2, bloco=24, B=1000, seed=0):
    y = np.asarray(y, bool)
    s1, s2 = np.nan_to_num(np.asarray(s1, float)), np.nan_to_num(np.asarray(s2, float))
    idx = _indices_bloco(len(y), bloco, B, np.random.default_rng(seed))
    dif = []
    for i in idx:
        if 0 < y[i].sum() < len(i):
            dif.append(roc_auc_score(y[i], s1[i]) - roc_auc_score(y[i], s2[i]))
    dif = np.array(dif)
    est = roc_auc_score(y, s1) - roc_auc_score(y, s2)
    return est, np.quantile(dif, 0.025), np.quantile(dif, 0.975)


def comparar_pareado(y, p1, p2):
    y, p1, p2 = (np.asarray(a, bool) for a in (y, p1, p2))
    saida = {}
    for nome, m in (("deteccao_nos_positivos", y), ("falsos_alarmes_nos_negativos", ~y)):
        a, b = p1[m], p2[m]
        n10, n01 = int((a & ~b).sum()), int((~a & b).sum())
        pv = stats.binomtest(min(n10, n01), n10 + n01, 0.5).pvalue if n10 + n01 else 1.0
        saida[nome] = {"so_modelo1": n10, "so_modelo2": n01, "p_mcnemar_exato": pv}
    return saida


def holm(pvals):
    p = np.asarray(pvals, float)
    ordem = np.argsort(p)
    aj = np.empty_like(p)
    maximo = 0.0
    for r, i in enumerate(ordem):
        maximo = max(maximo, (len(p) - r) * p[i])
        aj[i] = min(1.0, maximo)
    return aj


# ---------- calibração de metas ----------

def grade_limiares(w, chuvas, mares, beta=2.0, col_chuva="chuva_max", col_mare="mare_h"):
    y = w["y"].to_numpy()
    linhas = []
    for c in chuvas:
        for m in mares:
            p = ((w[col_chuva] >= c) & (w[col_mare] >= m)).to_numpy()
            r = metricas(y, p)
            pr, po = r["PREC"], r["POD"]
            den = beta**2 * pr + po
            r["Fbeta"] = (1 + beta**2) * pr * po / den if den and den == den else np.nan
            linhas.append({"chuva_mm": c, "mare_m": m, **r})
    return pd.DataFrame(linhas)


def selecionar_e_testar(w, corte, chuvas, mares, criterio="TSS", col_chuva="chuva_max",
                        col_mare="mare_h"):
    tr, te = w[w.index < corte], w[w.index >= corte]
    g = grade_limiares(tr, chuvas, mares, col_chuva=col_chuva, col_mare=col_mare)
    melhor = g.sort_values(criterio, ascending=False).iloc[0]
    c, m = melhor["chuva_mm"], melhor["mare_m"]
    p_te = ((te[col_chuva] >= c) & (te[col_mare] >= m)).to_numpy()
    return {"chuva_mm": c, "mare_m": m, f"{criterio}_treino": melhor[criterio],
            "teste": metricas(te["y"].to_numpy(), p_te), "n_teste": len(te),
            "eventos_teste": int(te["y"].sum())}


def rolling_origin(w, chuvas, mares, n_folds=4, criterio="TSS", **kw):
    cortes = np.linspace(0.5, 1.0, n_folds + 1)[:-1]
    saida = []
    for k, f in enumerate(cortes):
        i = int(len(w) * f)
        j = int(len(w) * (f + 0.5 / n_folds))
        sub = w.iloc[:j]
        r = selecionar_e_testar(sub, w.index[i], chuvas, mares, criterio, **kw)
        saida.append({"dobra": k, "chuva_mm": r["chuva_mm"], "mare_m": r["mare_m"],
                      "n_teste": r["n_teste"], "eventos_teste": r["eventos_teste"],
                      **{k2: r["teste"][k2] for k2 in ("POD", "FAR", "CSI", "TSS")}})
    return pd.DataFrame(saida)


# ---------- efeito composto: regressão logística de Firth ----------

def firth_logit(X, y, maxit=100, tol=1e-8):
    X = np.column_stack([np.ones(len(X)), np.asarray(X, float)])
    y = np.asarray(y, float)
    beta = np.zeros(X.shape[1])
    for _ in range(maxit):
        p = 1 / (1 + np.exp(-np.clip(X @ beta, -30, 30)))
        W = p * (1 - p)
        inv = np.linalg.pinv(X.T @ (X * W[:, None]))
        h = np.einsum("ij,jk,ik->i", X, inv, X) * W
        passo = np.clip(inv @ (X.T @ (y - p + h * (0.5 - p))), -1, 1)
        beta = beta + passo
        if np.max(np.abs(passo)) < tol:
            break
    eta = np.clip(X @ beta, -30, 30)
    p = 1 / (1 + np.exp(-eta))
    info = X.T @ (X * (p * (1 - p))[:, None])
    ll = float(np.sum(y * eta - np.log1p(np.exp(eta))) + 0.5 * np.linalg.slogdet(info)[1])
    return beta, np.sqrt(np.diag(np.linalg.pinv(info))), ll


def _preditores(w, interacao):
    c = np.log1p(w["chuva_max"].fillna(0).to_numpy())
    m = w["mare_h"].fillna(0).to_numpy()
    c, m = (c - c.mean()) / (c.std() or 1), (m - m.mean()) / (m.std() or 1)
    return np.column_stack([c, m, c * m]) if interacao else np.column_stack([c, m])


def teste_interacao(w):
    y = w["y"].to_numpy()
    b0, _, ll0 = firth_logit(_preditores(w, False), y)
    b1, se1, ll1 = firth_logit(_preditores(w, True), y)
    lrt = max(0.0, 2 * (ll1 - ll0))
    tab = pd.DataFrame({"coef": b1, "OR": np.exp(b1), "se": se1},
                       index=["intercepto", "chuva", "mare", "chuva_x_mare"])
    return tab, lrt, float(stats.chi2.sf(lrt, 1))


def prob_temporal(w, interacao=True, n_folds=4):
    y = w["y"].to_numpy()
    X = _preditores(w, interacao)
    prob = np.full(len(w), np.nan)
    cortes = np.linspace(0.5, 1.0, n_folds + 1)
    for a, b in zip(cortes[:-1], cortes[1:]):
        i, j = int(len(w) * a), int(len(w) * b)
        beta, _, _ = firth_logit(X[:i], y[:i])
        prob[i:j] = 1 / (1 + np.exp(-(beta[0] + X[i:j] @ beta[1:])))
    return prob


def calibracao(y, prob, bins=8):
    ok = ~np.isnan(prob)
    y, prob = np.asarray(y, float)[ok], prob[ok]
    brier = float(np.mean((prob - y) ** 2))
    ref = float(y.mean() * (1 - y.mean()))
    q = pd.qcut(prob, bins, duplicates="drop")
    tab = pd.DataFrame({"prob": prob, "obs": y}).groupby(q, observed=True).agg(
        prob_media=("prob", "mean"), freq_obs=("obs", "mean"), n=("obs", "size"))
    return tab, brier, (1 - brier / ref) if ref else np.nan


def risco_relativo(w, c_forte=5.0, m_alta=2.0):
    forte = (w["chuva_max"] >= c_forte).to_numpy()
    alta = (w["mare_h"] >= m_alta).to_numpy()
    y = w["y"].to_numpy()
    ka, na = int(y[forte & alta].sum()), int((forte & alta).sum())
    kb, nb = int(y[forte & ~alta].sum()), int((forte & ~alta).sum())
    if min(na, nb) == 0:
        return {"ka": ka, "na": na, "kb": kb, "nb": nb, "RR": np.nan}
    corr = 0.5 if 0 in (ka, kb) else 0.0
    a, b_, n1, n2 = ka + corr, kb + corr, na + 2 * corr, nb + 2 * corr
    rr = (a / n1) / (b_ / n2)
    se = np.sqrt(1 / a - 1 / n1 + 1 / b_ - 1 / n2)
    return {"ka": ka, "na": na, "kb": kb, "nb": nb, "RR": rr,
            "ic95": (rr * np.exp(-1.96 * se), rr * np.exp(1.96 * se)),
            "correcao_0.5": bool(corr),
            "p_fisher": stats.fisher_exact([[ka, na - ka], [kb, nb - kb]])[1]}


# ---------- eventos, antecedência, tamanho amostral ----------

def agrupar_eventos(inicios, gap="6h"):
    t = pd.Series(sorted(inicios))
    if t.empty:
        return t
    return t[t.diff().isna() | (t.diff() > pd.Timedelta(gap))].reset_index(drop=True)


def avaliar_eventos(w, inicios, nivel=2, janela="1h"):
    flag = (w["risco"] >= nivel).reset_index(drop=True)
    idx = w.index
    grupos = (flag != flag.shift()).cumsum()
    ev = agrupar_eventos(inicios)
    leads = []
    for e in ev:
        k = idx.searchsorted(e, side="right") - 1
        lead = np.nan
        for kk in (k, k - 1):
            if 0 <= kk < len(flag) and flag[kk]:
                ini = np.flatnonzero((grupos == grupos[kk]).to_numpy())[0]
                lead = (e - idx[ini]).total_seconds() / 60
                break
        leads.append(lead)
    leads = pd.Series(leads, dtype=float)
    ep = flag[flag].groupby(grupos[flag]).apply(lambda s: (idx[s.index[0]], idx[s.index[-1]]))
    ts = ev.tolist()
    acertos = sum(any(a <= e <= b + pd.Timedelta(janela) for e in ts) for a, b in ep)
    return {"eventos": len(ev), "detectados": int(leads.notna().sum()),
            "POD_evento": leads.notna().mean() if len(ev) else np.nan,
            "antecedencia_min_mediana": leads.median(),
            "antecedencia_min_q1_q3": (leads.quantile(0.25), leads.quantile(0.75)),
            "episodios_alerta": len(ep),
            "FAR_episodio": 1 - acertos / len(ep) if len(ep) else np.nan}


def checar_amostra(y, n_preditores=3):
    e = int(np.asarray(y).sum())
    return {"eventos": e, "eventos_por_preditor": e / n_preditores,
            "suficiente_p_modelo": e >= 10 * n_preditores}


def n_para_ic(p, largura, a=0.05):
    z = stats.norm.ppf(1 - a / 2)
    return int(np.ceil(4 * z * z * p * (1 - p) / largura**2))
