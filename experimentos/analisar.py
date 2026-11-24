"""
analisar.py - resume os CSVs brutos e gera os graficos.

    python experimentos/analisar.py

Le dados/<so>/vazao.csv e dados/<so>/latencia.csv(.gz) sem alterar
nada, e escreve:
    experimentos/resumo.csv
    experimentos/resumo_latencia.csv
    experimentos/graficos/vazao_<so>.png
    experimentos/graficos/latencia.png
"""
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import pandas as pd

RAIZ = Path(__file__).resolve().parent.parent
DADOS = RAIZ / "dados"
SAIDA = RAIZ / "experimentos"
GRAF = SAIDA / "graficos"
CHAVE = ["so", "versao", "produtores", "consumidores", "buffer"]


def ler_vazao():
    partes = [pd.read_csv(p) for p in sorted(DADOS.glob("*/vazao.csv"))]
    if not partes:
        raise SystemExit("nenhum dados/<so>/vazao.csv encontrado")
    df = pd.concat(partes, ignore_index=True)
    return df[df["rep"] > 0]  # rep 0 seria aquecimento, por garantia


def ler_latencia():
    partes = []
    for so_dir in sorted(p for p in DADOS.iterdir() if p.is_dir()):
        for nome in ("latencia.csv.gz", "latencia.csv"):
            arq = so_dir / nome
            if arq.exists():
                partes.append(pd.read_csv(arq))
                break
    return pd.concat(partes, ignore_index=True) if partes else None


def resumo(df):
    g = df.groupby(CHAVE)
    r = g["vazao_itens_s"].agg(media="mean", desvio="std", n="count").reset_index()
    t = g["tempo_s"].agg(tempo_medio="mean", tempo_desvio="std").reset_index()
    r = r.merge(t, on=CHAVE)
    r.to_csv(SAIDA / "resumo.csv", index=False, float_format="%.4f")
    return r


def grafico_vazao(r):
    for so, d_so in r.groupby("so"):
        grupos = list(d_so.groupby(["produtores", "consumidores"]))
        fig, axes = plt.subplots(1, len(grupos), figsize=(5 * len(grupos), 4), squeeze=False)
        for ax, ((p, c), d_pc) in zip(axes[0], grupos):
            for versao, d in d_pc.groupby("versao"):
                d = d.sort_values("buffer")
                rot = "A (var. condicao)" if versao == "A" else "B (semaforo)"
                ax.errorbar(d["buffer"], d["media"], yerr=d["desvio"],
                            marker="o", capsize=4, label=rot)
            ax.set_xscale("log", base=2)
            ax.set_xticks([1, 16, 256])
            ax.set_xticklabels(["1", "16", "256"])
            ax.set_xlabel("tamanho do buffer")
            ax.set_ylabel("vazao (itens/s)")
            ax.set_title(f"{p} produtor(es) x {c} consumidor(es)")
            ax.grid(alpha=0.3)
            ax.legend()
        fig.suptitle(f"Vazao x tamanho do buffer - {so}")
        fig.tight_layout()
        fig.savefig(GRAF / f"vazao_{so}.png", dpi=150)
        plt.close(fig)


def grafico_latencia(lat):
    grupos, rotulos = [], []
    for (so, versao), d in lat.groupby(["so", "versao"]):
        grupos.append(d["latencia_ns"] / 1000.0)
        rotulos.append(f"{so}\n{versao}")
    fig, ax = plt.subplots(figsize=(8, 4.5))
    ax.boxplot(grupos, showfliers=False)
    ax.set_xticks(range(1, len(rotulos) + 1))
    ax.set_xticklabels(rotulos)
    ax.set_ylabel("latencia (us)")
    ax.set_title("Latencia produtor -> consumidor (1x1, buffer 1)")
    ax.grid(axis="y", alpha=0.3)
    fig.tight_layout()
    fig.savefig(GRAF / "latencia.png", dpi=150)
    plt.close(fig)

    q = lat.groupby(["so", "versao"])["latencia_ns"].describe(percentiles=[0.5, 0.9, 0.99])
    q.to_csv(SAIDA / "resumo_latencia.csv", float_format="%.1f")


def main():
    GRAF.mkdir(parents=True, exist_ok=True)
    r = resumo(ler_vazao())
    grafico_vazao(r)
    print(r.to_string(index=False))

    lat = ler_latencia()
    if lat is not None:
        grafico_latencia(lat)
    else:
        print("sem dados de latencia ainda")


if __name__ == "__main__":
    main()
