"""Une logs de varios coletores (VM, PC, nuvem) em um so, sem duplicar leituras simultaneas.

    python mesclar_logs.py vm.csv pc.csv nuvem.csv -o ../data/leituras_mescladas.csv [--bucket 60]

Cada arquivo vira um valor da coluna 'coletor'. Leituras de coletores diferentes dentro do mesmo
balde de `--bucket` segundos sao colapsadas (fica a que tem chuva_ok=1). IMPORTANTE: as colunas de
risco/alerta de coletores secundarios usam o estado de limiares DELES; na analise, recalcule o risco
a partir de chuva e mare (sensor_stats.risco_offline) usando os limiares do coletor principal.
"""
import argparse
import os

import pandas as pd


def mesclar(arquivos, bucket=60):
    partes = []
    for a in arquivos:
        d = pd.read_csv(a)
        d["coletor"] = os.path.splitext(os.path.basename(a))[0]
        partes.append(d)
    df = pd.concat(partes, ignore_index=True, sort=False)
    df["_b"] = df["epoch"] // max(bucket, 1)
    df = df.sort_values(["_b", "chuva_ok"], ascending=[True, False])
    df = df.drop_duplicates("_b", keep="first").drop(columns="_b").sort_values("epoch")
    return df.reset_index(drop=True)


def cobertura(df, intervalo_s):
    esperado = (df["epoch"].max() - df["epoch"].min()) / intervalo_s + 1
    return len(df) / esperado


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("arquivos", nargs="+")
    ap.add_argument("-o", "--saida", required=True)
    ap.add_argument("--bucket", type=int, default=60)
    ap.add_argument("--intervalo", type=int, default=600)
    a = ap.parse_args()
    for f in a.arquivos:
        d = pd.read_csv(f, usecols=["epoch"])
        print(f"{f}: {len(d)} leituras, cobertura {cobertura(d, a.intervalo):.1%} (intervalo {a.intervalo}s)")
    m = mesclar(a.arquivos, a.bucket)
    m.to_csv(a.saida, index=False)
    print(f"mesclado: {len(m)} leituras, cobertura {cobertura(m, a.intervalo):.1%} -> {a.saida}")


if __name__ == "__main__":
    main()
