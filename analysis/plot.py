from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

import numpy as np

def _load(path: Path) -> dict:
    with open(path) as f:
        return json.load(f)

def _reductions_from_results(
    data: dict, algos: list[str]
) -> tuple[dict[str, list[float]], dict[str, list[float]]]:
    traces: dict = data.get("traces")
    large_cache_size: dict[str, list[float]] = {a: [] for a in algos}
    small_cache_size: dict[str, list[float]] = {a: [] for a in algos}

    for _name, entry in traces.items():
        large_size = entry.get("large_size")
        small_size = entry.get("small_size")
        miss_ratios = entry.get("miss_ratios")

        if large_size is None or small_size is None:
            continue

        fifo = miss_ratios.get("fifo")
        fifo_large = fifo.get(str(large_size))
        fifo_small = fifo.get(str(small_size))
        if not fifo_large and not fifo_small:
            continue

        for algo in algos:
            algo_mrs = miss_ratios.get(algo)
            if fifo_large and str(large_size) in algo_mrs:
                mr = algo_mrs[str(large_size)]
                large_cache_size[algo].append((fifo_large - mr) / fifo_large)
            if fifo_small and str(small_size) in algo_mrs:
                small_cache_size[algo].append((fifo_small - mr) / fifo_small)

    return large_cache_size, small_cache_size

def plot(
    large_cache_size: dict[str, list[float]],
    small_cache_size: dict[str, list[float]],
    algos: list[str],
    output: Path,
) -> None:
    labels = algos
    positions = list(range(len(algos)))
    fig_width = max(5, 1.1 * len(algos) + 2)

    fig, (ax_c, ax_f) = plt.subplots(1, 2, figsize=(fig_width * 2, 4.5))

    n_large = max((len(v) for v in large_cache_size.values()))
    n_small = max((len(v) for v in small_cache_size.values()))

    panels = [
        (
            ax_c,
            large_cache_size,
            f"Twitter workloads, large cache, {n_large} traces",
            "#E8A020",
        ),
        (
            ax_f,
            small_cache_size,
            f"Twitter workloads, small cache, {n_small} traces",
            "#6495CD",
        ),
    ]

    for ax, reductions, title, color in panels:
        data = [reductions.get(a) for a in algos]

        ax.boxplot(
            data,
            positions=positions,
            widths=0.5,
            patch_artist=True,
            showfliers=False,
            whis=[10, 90],
            medianprops=dict(color="black", linewidth=1.2),
            boxprops=dict(facecolor=color, alpha=0.85),
            whiskerprops=dict(color="black", linewidth=0.8),
            capprops=dict(color="black", linewidth=0.8),
        )

        for i, vals in enumerate(data):
            if vals:
                ax.plot(
                    i,
                    float(np.mean(vals)),
                    marker="v",
                    color="#1A6B28",
                    markersize=7,
                    linestyle="none",
                    zorder=5,
                )

        ax.set_xticks(positions)
        ax.set_xticklabels(labels, rotation=45, ha="right", fontsize=9)
        ax.set_ylabel("Miss Ratio Reduction\nfrom FIFO", fontsize=9)
        ax.set_title(title, fontsize=9, pad=4)
        ax.grid(axis="y", linestyle="--", alpha=0.4, zorder=0)
        ax.axhline(0, color="black", linewidth=0.5, zorder=1)
        ax.tick_params(axis="both", labelsize=8)    

    plt.tight_layout()
    output.parent.mkdir(parents=True, exist_ok=True)
    plt.savefig(output, bbox_inches="tight", dpi=150)
    print(f"Figure saved to {output}")

def main() -> None:
    p = argparse.ArgumentParser(
        description="Plot SIEVE figure 3c and 3f from a JSON results file"
    )
    p.add_argument("--input", required=True, help="Input JSON")
    p.add_argument("--output", default="results/fig3cf.pdf", help="Output PDF path")
    args = p.parse_args()

    data = _load(Path(args.input))
    algos = [a for a in data.get("meta", {}).get("algos", []) if a != "fifo"]

    large_cache_size, small_cache_size = _reductions_from_results(data, algos)
    plot(large_cache_size, small_cache_size, algos, Path(args.output))

if __name__ == "__main__":
    main()
