"""Checagens de sanidade do experimento: a coleta e valida? o sensor faz sentido?

Uso:
  python checar_experimento.py --log ../data/leituras_log.csv --tabua ../data/mare_recife.csv \
      [--ocorrencias ocorrencias.csv] [--feedback ../data/feedback_log.csv]

Cada checagem imprime OK / ATENCAO / FALHA. Saida em resultados/checagem.md.
Codigo de saida: 1 se houver FALHA.
"""
import argparse
import os
import sys

import numpy as np
import pandas as pd

import sensor_stats as ss

LINHAS = []
FALHAS = 0


def reg(nivel, titulo, detalhe=""):
    global FALHAS
    if nivel == "FALHA":
        FALHAS += 1
    LINHAS.append((nivel, titulo, detalhe))
    print(f"[{nivel:7s}] {titulo}" + (f" -- {detalhe}" if detalhe else ""))


# ---------- 1. cobertura e interrupcoes ----------

def cobertura(df, intervalo_s):
    dt = df.index.to_series().diff().dt.total_seconds().dropna()
    dur_h = (df.index[-1] - df.index[0]).total_seconds() / 3600
    esperado = (df.index[-1] - df.index[0]).total_seconds() / intervalo_s + 1
    cob = len(df) / esperado
    lac = dt[dt > 3 * intervalo_s]
    tempo_fora_h = float((lac - intervalo_s).sum()) / 3600
    reg("OK" if cob >= 0.8 else "ATENCAO" if cob >= 0.5 else "FALHA",
        "cobertura temporal", f"{cob:.1%} do esperado em {dur_h / 24:.1f} dias "
        f"({len(df)} leituras, intervalo mediano {intervalo_s:.0f} s)")
    reg("OK" if len(lac) == 0 else "ATENCAO", "interrupcoes",
        f"{len(lac)} lacunas > 3x intervalo; fora do ar {tempo_fora_h:.1f} h; "
        f"maior lacuna {lac.max() / 3600 if len(lac) else 0:.1f} h")
    if "boot_id" in df:
        reinicios = int(df["boot_id"].nunique() - 1)
        saltos = 0
        for _, g in df.groupby("boot_id"):
            saltos += int((g["seq"].diff().dropna() != 1).sum())
        reg("OK" if saltos == 0 else "ATENCAO", "continuidade da sequencia",
            f"{reinicios} reinicios do programa; {saltos} saltos de seq dentro de uma execucao")
    return lac


def vies_de_cobertura(df, intervalo_s, lim_mare):
    """Se o PC fica ligado em horarios/condicoes especificas, a amostra e enviesada."""
    h = df.index.floor("1h")
    por_hora = df.groupby(h).size() / (3600 / intervalo_s)
    por_hora = por_hora.clip(upper=1.0)
    grade = pd.date_range(h.min(), h.max(), freq="1h")
    cob_h = por_hora.reindex(grade).fillna(0.0)
    hora_dia = cob_h.groupby(cob_h.index.hour).mean()
    reg("OK" if hora_dia.min() >= 0.5 * hora_dia.max() else "ATENCAO",
        "cobertura por hora do dia",
        f"min {hora_dia.min():.0%} (h={hora_dia.idxmin()}), max {hora_dia.max():.0%} (h={hora_dia.idxmax()})")
    mare_h = df["mare_interp_m"].resample("1h").max().reindex(grade)
    alta = (mare_h >= lim_mare)
    if alta.sum() > 5 and (~alta & mare_h.notna()).sum() > 5:
        ca, cb = cob_h[alta.fillna(False)].mean(), cob_h[(~alta) & mare_h.notna()].mean()
        rel = abs(ca - cb) / max(ca, cb, 1e-9)
    else:
        ca = cb = rel = float("nan")
    reg("OK" if not rel > 0.2 else "ATENCAO", "cobertura x fase da mare",
        f"horas de mare alta: {ca:.0%} cobertas | demais: {cb:.0%}")
    chuva_h = df["chuva_media_mm"].resample("1h").max().reindex(grade)
    chove = chuva_h > 0
    if chove.sum() > 5:
        cc, cs = cob_h[chove.fillna(False)].mean(), cob_h[~chove.fillna(False)].mean()
        reg("OK" if abs(cc - cs) / max(cc, cs, 1e-9) <= 0.2 else "ATENCAO", "cobertura x chuva",
            f"horas com chuva: {cc:.0%} cobertas | sem chuva: {cs:.0%} "
            "(se muito menor com chuva, a amostra perde justamente os eventos)")
    return cob_h


# ---------- 2. fonte de dados ----------

def fonte(df):
    ok = df["chuva_ok"].mean()
    reg("OK" if ok >= 0.95 else "ATENCAO" if ok >= 0.8 else "FALHA", "feed de chuva disponivel", f"{ok:.1%}")
    if "http_code" in df:
        codigos = df["http_code"].value_counts().to_dict()
        reg("OK" if df["http_code"].eq(200).mean() >= 0.95 else "ATENCAO", "codigos HTTP", str(codigos))
    if "idade_dado_s" in df:
        idade = df["idade_dado_s"].replace(0, np.nan).dropna()
        if len(idade):
            reg("OK" if idade.median() < 1800 else "ATENCAO", "idade do dado na fonte",
                f"mediana {idade.median() / 60:.1f} min, p95 {idade.quantile(.95) / 60:.1f} min")
    ult = df.loc[df["chuva_ok"] == 1, "chuva_ultima_leitura"]
    novos = ult[ult != ult.shift()]
    if len(novos) > 3:
        cad = pd.to_datetime(novos).diff().dt.total_seconds().dropna()
        reg("OK", "cadencia da fonte (mudancas de 'ultima_leitura')",
            f"mediana {cad.median() / 60:.1f} min entre atualizacoes")
    plaus = df["chuva_media_mm"].dropna()
    reg("OK" if plaus.between(0, 300).all() else "FALHA", "chuva fisicamente plausivel",
        f"min {plaus.min():.2f}, max {plaus.max():.2f} mm")


# ---------- 3. mare e logica ----------

def mare_e_logica(df, tabua):
    u = df[df["mare_ok"] == 1]
    ref = ss.mare_interpolada(tabua, u.index)
    dif = (u["mare_interp_m"] - ref).abs().dropna()
    reg("OK" if dif.quantile(.99) <= 0.02 else "FALHA", "mare do C == mare recomputada em Python",
        f"p99 |dif| = {dif.quantile(.99):.4f} m, max {dif.max():.4f} m ({len(dif)} leituras)")
    h = u["mare_interp_m"].dropna()
    reg("OK" if h.between(-0.5, 3.5).all() else "FALHA", "mare fisicamente plausivel",
        f"min {h.min():.2f}, max {h.max():.2f} m")
    if {"limiar_chuva_mm", "limiar_mare_m", "alerta"} <= set(df.columns):
        dispensa = df["limiar_chuva_mm"] <= 0
        regra = ((df["chuva_media_mm"] >= df["limiar_chuva_mm"]) | (dispensa & df["chuva_ok"].eq(0))) \
            & (df["mare_interp_m"] >= df["limiar_mare_m"])
        valido = (df["mare_ok"] == 1) & ((df["chuva_ok"] == 1) | dispensa)
        incons = int((regra[valido].astype(int) != df.loc[valido, "alerta"]).sum())
        reg("OK" if incons <= 0.001 * valido.sum() else "FALHA",
            "alerta registrado == regra recalculada com os limiares do log",
            f"{incons} divergencias em {int(valido.sum())} leituras")
        taxa = df.loc[valido, "alerta"].mean()
        reg("OK" if 0 < taxa < 0.05 else "ATENCAO", "taxa de alerta",
            f"{taxa:.3%} das leituras (esperado: evento raro; ~0 = limiar alto demais; >5% = baixo demais)")
        alta = (df.loc[valido, "mare_interp_m"] >= df.loc[valido, "limiar_mare_m"]).mean()
        reg("OK" if alta < 0.35 else "ATENCAO", "fracao do tempo com mare >= limiar",
            f"{alta:.1%} (a versao antiga 'posicao >= 0.75' dava ~50%)")
        sem_chuva = int(((df["alerta"] == 1) & (df["chuva_media_mm"] <= 0) & (df["limiar_chuva_mm"] > 0)).sum())
        reg("OK" if sem_chuva == 0 else "FALHA", "controle negativo: alerta sem chuva", f"{sem_chuva} casos")
        lim = df[["limiar_chuva_mm", "limiar_mare_m"]].drop_duplicates()
        reg("OK", "limiares ao longo do tempo",
            f"{len(lim)} combinacoes distintas; ultimo = ({df['limiar_chuva_mm'].iloc[-1]:.2f} mm, "
            f"{df['limiar_mare_m'].iloc[-1]:.2f} m)")


# ---------- 4. eventos documentados e placebo ----------

def _y_grade(grade, oc, janela):
    ini = np.sort(ss._ns(oc["inicio"]))
    lo = ss._ns(grade)
    hi = lo + pd.Timedelta(janela).value
    return (np.searchsorted(ini, hi) - np.searchsorted(ini, lo)) > 0


def eventos_e_placebo(df, oc, janela="1h", intervalo_s=30, K=2000, seed=0):
    grade = pd.date_range(df.index.min().floor(janela), df.index.max().floor(janela), freq=janela)
    cobertura_h = df.groupby(df.index.floor(janela)).size().reindex(grade).fillna(0)
    coberta = (cobertura_h >= 0.5 * pd.Timedelta(janela).total_seconds() / intervalo_s).to_numpy()
    alerta = (df["risco_codigo"].resample(janela).max().reindex(grade) >= 2).to_numpy()
    y = _y_grade(grade, oc, janela)
    ini = df.index.min(); fim = df.index.max()
    dentro = oc[(oc["inicio"] >= ini) & (oc["inicio"] <= fim)]
    cobertas = sum(bool(coberta[max(0, int(np.searchsorted(grade, t.floor(janela))))
                               if t.floor(janela) in grade else 0]) for t in dentro["inicio"])
    reg("OK" if len(dentro) and cobertas / len(dentro) >= 0.8 else "ATENCAO" if len(dentro) else "FALHA",
        "ocorrencias dentro do periodo coletado e com dados no momento",
        f"{len(dentro)} no periodo, {cobertas} com cobertura >=50% na janela")
    if y[coberta].sum() < 5:
        reg("ATENCAO", "placebo (deslocamento temporal)", f"so {int(y[coberta].sum())} janelas positivas: sem poder")
        return

    def csi(yy):
        a, v = alerta[coberta], yy[coberta]
        tp, fp, fn = int((a & v).sum()), int((a & ~v).sum()), int((~a & v).sum())
        return tp / (tp + fp + fn) if (tp + fp + fn) else 0.0

    obs = csi(y)
    rng = np.random.default_rng(seed)
    n = len(y)
    minimo = int(48 * 3600 / pd.Timedelta(janela).total_seconds())
    desl = rng.integers(minimo, max(minimo + 1, n - minimo), K)
    nulo = np.array([csi(np.roll(y, int(k))) for k in desl])
    p = (1 + (nulo >= obs).sum()) / (1 + K)
    reg("OK" if p < 0.05 else "ATENCAO", "placebo: CSI observado vs ocorrencias deslocadas no tempo",
        f"CSI={obs:.3f}; mediana do nulo={np.median(nulo):.3f}; p={p:.4f} "
        "(p alto => o sensor nao acerta mais que o acaso; mantem a autocorrelacao das series)")


# ---------- 5. feedback ----------

def feedback(caminho):
    fb = pd.read_csv(caminho)
    if fb.empty:
        reg("ATENCAO", "feedback", "arquivo vazio")
        return
    ac = fb[fb["aceito"] == 1]
    reg("OK" if len(ac) else "ATENCAO", "feedback recebido",
        f"{len(fb)} mensagens, {len(ac)} aceitas ({len(ac) / len(fb):.0%}); "
        f"rejeicoes: {fb.loc[fb.aceito == 0, 'rejeicao'].value_counts().to_dict()}")
    if len(ac):
        top = ac["usuario"].value_counts(normalize=True).iloc[0]
        reg("OK" if top <= 0.5 or ac["usuario"].nunique() <= 1 and len(ac) < 5 else "ATENCAO",
            "concentracao de usuarios", f"{ac['usuario'].nunique()} usuarios; o maior responde por {top:.0%} dos aceitos")
        reg("OK", "tipos de confirmacao", str(ac["tipo"].value_counts().to_dict()))
        lr = ac["lim_chuva_depois"]; lh = ac["lim_mare_depois"]
        no_limite = ((lr <= 0.0 + 1e-6) | (lr >= 50 - 1e-6) | (lh <= 1.0 + 1e-6) | (lh >= 2.7 - 1e-6)).mean()
        reg("OK" if no_limite < 0.2 else "ATENCAO", "limiares encostados nos limites", f"{no_limite:.0%} das atualizacoes")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--log", required=True)
    ap.add_argument("--tabua", default="../data/mare_recife.csv")
    ap.add_argument("--ocorrencias")
    ap.add_argument("--feedback")
    ap.add_argument("--saida", default="resultados")
    a = ap.parse_args()

    df = ss.carregar_log(a.log)
    if len(df) < 10:
        reg("FALHA", "log", f"apenas {len(df)} leituras")
        sys.exit(1)
    intervalo = float(df.index.to_series().diff().dt.total_seconds().median())
    tabua = ss.carregar_tabua(a.tabua)
    lim_mare = float(df["limiar_mare_m"].median()) if "limiar_mare_m" in df else 2.0

    print(f"== Checagem de {a.log} ==")
    cobertura(df, intervalo)
    vies_de_cobertura(df, intervalo, lim_mare)
    fonte(df)
    mare_e_logica(df, tabua)
    if a.ocorrencias and os.path.exists(a.ocorrencias):
        eventos_e_placebo(df, ss.carregar_ocorrencias(a.ocorrencias), intervalo_s=intervalo)
    else:
        reg("ATENCAO", "ocorrencias documentadas", "nenhum arquivo: sem validacao contra eventos")
    if a.feedback and os.path.exists(a.feedback):
        feedback(a.feedback)

    os.makedirs(a.saida, exist_ok=True)
    with open(f"{a.saida}/checagem.md", "w", encoding="utf-8") as f:
        f.write(f"# Checagem do experimento\n\nArquivo: `{a.log}`\n\n| nivel | checagem | detalhe |\n|---|---|---|\n")
        for n, t, d in LINHAS:
            f.write(f"| {n} | {t} | {d} |\n")
    print(f"\n{FALHAS} falha(s). Relatorio em {a.saida}/checagem.md")
    sys.exit(1 if FALHAS else 0)


if __name__ == "__main__":
    main()
