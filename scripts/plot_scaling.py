#!/usr/bin/env python3
"""
Generate publication-quality graphs for multi-threaded scaling evaluation.
Compares FIFO, SIEVE, and LRU across thread counts.

Usage:
    python3 scripts/plot_scaling.py --input results_scaling_sweep.json
"""
import argparse
import json
from collections import defaultdict
from pathlib import Path
from statistics import mean

import matplotlib.pyplot as plt
import numpy as np

# ── Style ────────────────────────────────────────────────────────────────────
plt.rcParams.update({
    "figure.figsize": (7, 4.5),
    "figure.dpi": 150,
    "font.size": 11,
    "axes.titlesize": 13,
    "axes.labelsize": 12,
    "legend.fontsize": 10,
    "xtick.labelsize": 10,
    "ytick.labelsize": 10,
    "axes.grid": True,
    "grid.alpha": 0.25,
    "lines.linewidth": 2,
    "lines.markersize": 7,
    "savefig.bbox": "tight",
    "savefig.pad_inches": 0.15,
})

CACHE_STYLES = {
    "FIFO":  {"color": "#2ca02c", "marker": "D"},
    "SIEVE": {"color": "#1f77b4", "marker": "o"},
    "LRU":   {"color": "#d62728", "marker": "s"},
}
THREAD_COUNTS = [1, 2, 4, 8, 16]


def style_for(name: str) -> dict:
    return CACHE_STYLES.get(name, {"color": "#333333", "marker": "^"})


def fmt_qps(v: float) -> str:
    if v >= 1e9: return f"{v/1e9:.1f}B"
    if v >= 1e6: return f"{v/1e6:.1f}M"
    if v >= 1e3: return f"{v/1e3:.1f}K"
    return f"{v:.0f}"


# ── Data loading ─────────────────────────────────────────────────────────────
def load_records(paths: list[Path]) -> list[dict]:
    records = []
    for path in paths:
        with path.open() as f:
            for line in f:
                line = line.strip()
                if line:
                    records.append(json.loads(line))
    return records


def aggregate(records: list[dict]) -> list[dict]:
    """Aggregate batches → one row per (cache, trace, threads)."""
    groups: dict[tuple, list[dict]] = defaultdict(list)
    for r in records:
        key = (r["cache_name"], r["trace_name"], int(r["threads"]))
        groups[key].append(r)

    out = []
    for (cache, trace, threads), rows in sorted(groups.items()):
        all_samples = []
        for r in rows:
            all_samples.extend(r.get("samples", []))
        pcts = {}
        if all_samples:
            arr = np.array(all_samples)
            for p in [50, 90, 95, 99]:
                pcts[f"p{p}"] = float(np.percentile(arr, p))
        out.append({
            "cache": cache, "trace": trace, "threads": threads,
            "capacity": int(mean(r["capacity"] for r in rows)),
            "hit_rate": mean(r["hit_rate"] for r in rows),
            "throughput": mean(r["throughput_qps"] for r in rows),
            "avg_latency": mean(r["avg_latency_ns"] for r in rows),
            "batches": len(rows),
            "samples": all_samples,
            **pcts,
        })
    return out


def split_by_trace(rows: list[dict]) -> dict[str, list[dict]]:
    by_trace: dict[str, list[dict]] = defaultdict(list)
    for r in rows:
        by_trace[r["trace"]].append(r)
    return dict(by_trace)


def split_by_cache(rows: list[dict]) -> dict[str, list[dict]]:
    by_cache: dict[str, list[dict]] = defaultdict(list)
    for r in rows:
        by_cache[r["cache"]].append(r)
    return dict(by_cache)


def _lines_vs_threads(ax, rows_by_cache: dict, ykey: str):
    for cache, crows in sorted(rows_by_cache.items()):
        crows.sort(key=lambda r: r["threads"])
        xs = [r["threads"] for r in crows]
        ys = [r[ykey] for r in crows]
        s = style_for(cache)
        ax.plot(xs, ys, marker=s["marker"], color=s["color"], label=cache)
    ax.set_xticks(THREAD_COUNTS)


# ── Graph 1: Throughput vs threads ───────────────────────────────────────────
def plot_throughput_vs_threads(agg: list[dict], out: Path):
    for trace, trows in split_by_trace(agg).items():
        fig, ax = plt.subplots()
        _lines_vs_threads(ax, split_by_cache(trows), "throughput")
        ax.set_xlabel("Threads")
        ax.set_ylabel("Throughput (queries/sec)")
        ax.set_title(f"Throughput vs Threads — {trace}")
        ax.yaxis.set_major_formatter(plt.FuncFormatter(lambda v, _: fmt_qps(v)))
        ax.legend()
        fig.savefig(out / f"throughput_vs_threads_{trace}.png")
        plt.close(fig)


# ── Graph 2: Throughput scaling efficiency ───────────────────────────────────
def plot_scaling_efficiency(agg: list[dict], out: Path):
    """Throughput normalized to single-thread performance (ideal = linear)."""
    for trace, trows in split_by_trace(agg).items():
        fig, ax = plt.subplots()
        by_cache = split_by_cache(trows)
        for cache, crows in sorted(by_cache.items()):
            crows.sort(key=lambda r: r["threads"])
            base = crows[0]["throughput"]
            xs = [r["threads"] for r in crows]
            ys = [r["throughput"] / base for r in crows]
            s = style_for(cache)
            ax.plot(xs, ys, marker=s["marker"], color=s["color"], label=cache)
        # Ideal linear line
        ax.plot(THREAD_COUNTS, THREAD_COUNTS, linestyle="--", color="gray",
                alpha=0.6, label="ideal linear")
        ax.set_xlabel("Threads")
        ax.set_ylabel("Speedup (relative to 1 thread)")
        ax.set_title(f"Scaling Efficiency — {trace}")
        ax.set_xticks(THREAD_COUNTS)
        ax.legend()
        fig.savefig(out / f"scaling_efficiency_{trace}.png")
        plt.close(fig)


# ── Graph 3: Hit rate vs threads ─────────────────────────────────────────────
def plot_hit_rate_vs_threads(agg: list[dict], out: Path):
    for trace, trows in split_by_trace(agg).items():
        fig, ax = plt.subplots()
        _lines_vs_threads(ax, split_by_cache(trows), "hit_rate")
        ax.set_xlabel("Threads")
        ax.set_ylabel("Hit Rate")
        ax.set_title(f"Hit Rate vs Threads — {trace}")
        ax.yaxis.set_major_formatter(plt.FuncFormatter(lambda v, _: f"{v:.0%}"))
        ax.legend()
        fig.savefig(out / f"hit_rate_vs_threads_{trace}.png")
        plt.close(fig)


# ── Graph 4: Average latency vs threads ──────────────────────────────────────
def plot_avg_latency_vs_threads(agg: list[dict], out: Path):
    for trace, trows in split_by_trace(agg).items():
        fig, ax = plt.subplots()
        _lines_vs_threads(ax, split_by_cache(trows), "avg_latency")
        ax.set_xlabel("Threads")
        ax.set_ylabel("Average Latency (ns)")
        ax.set_title(f"Average Latency vs Threads — {trace}")
        ax.legend()
        fig.savefig(out / f"avg_latency_vs_threads_{trace}.png")
        plt.close(fig)


# ── Graph 5: Latency percentiles vs threads ──────────────────────────────────
def plot_latency_percentiles_vs_threads(agg: list[dict], out: Path):
    pct_keys = ["p50", "p90", "p95", "p99"]
    pct_colors = {"p50": "#2ca02c", "p90": "#ff7f0e", "p95": "#d62728", "p99": "#9467bd"}

    for trace, trows in split_by_trace(agg).items():
        by_cache = split_by_cache(trows)
        fig, axes = plt.subplots(1, len(by_cache), figsize=(7 * len(by_cache), 4.5),
                                  sharey=True, squeeze=False)
        for ax, (cache, crows) in zip(axes[0], sorted(by_cache.items())):
            crows.sort(key=lambda r: r["threads"])
            xs = [r["threads"] for r in crows]
            for pk in pct_keys:
                if pk in crows[0]:
                    ys = [r[pk] for r in crows]
                    ax.plot(xs, ys, marker="o", color=pct_colors[pk], label=pk)
            ax.set_xlabel("Threads")
            ax.set_title(cache)
            ax.set_xticks(THREAD_COUNTS)
            ax.legend(fontsize=9)
        axes[0][0].set_ylabel("Latency (ns)")
        fig.suptitle(f"Latency Percentiles vs Threads — {trace}", y=1.02)
        fig.savefig(out / f"latency_percentiles_vs_threads_{trace}.png")
        plt.close(fig)


# ── Graph 6: Latency CDF at max threads ─────────────────────────────────────
def plot_latency_cdf(agg: list[dict], out: Path):
    for trace, trows in split_by_trace(agg).items():
        max_threads = max(r["threads"] for r in trows)
        fig, ax = plt.subplots()
        for cache, crows in sorted(split_by_cache(trows).items()):
            row = [r for r in crows if r["threads"] == max_threads]
            if not row or not row[0]["samples"]:
                continue
            samples = np.array(row[0]["samples"])
            sorted_s = np.sort(samples)
            cdf = np.arange(1, len(sorted_s) + 1) / len(sorted_s)
            s = style_for(cache)
            ax.plot(sorted_s, cdf, color=s["color"], label=cache, linewidth=1.5)
        ax.set_xlabel("Latency (ns)")
        ax.set_ylabel("CDF")
        ax.set_title(f"Latency CDF at {max_threads} threads — {trace}")
        # Clip at p99.5 for readability
        all_samples = []
        for r in trows:
            if r["threads"] == max_threads:
                all_samples.extend(r["samples"])
        if all_samples:
            ax.set_xlim(left=0, right=np.percentile(all_samples, 99.5))
        ax.legend()
        fig.savefig(out / f"latency_cdf_{max_threads}thr_{trace}.png")
        plt.close(fig)


# ── Graph 7: Throughput bar chart grouped by thread count ────────────────────
def plot_throughput_bars(agg: list[dict], out: Path):
    for trace, trows in split_by_trace(agg).items():
        by_cache = split_by_cache(trows)
        caches = sorted(by_cache.keys())
        threads_list = sorted(set(r["threads"] for r in trows))

        x = np.arange(len(threads_list))
        width = 0.8 / len(caches)

        fig, ax = plt.subplots()
        for i, cache in enumerate(caches):
            crows = sorted(by_cache[cache], key=lambda r: r["threads"])
            ys = [r["throughput"] for r in crows]
            s = style_for(cache)
            ax.bar(x + i * width, ys, width, label=cache, color=s["color"],
                   edgecolor="black", linewidth=0.4)

        ax.set_xlabel("Threads")
        ax.set_ylabel("Throughput (queries/sec)")
        ax.set_title(f"Throughput by Thread Count — {trace}")
        ax.set_xticks(x + width * (len(caches) - 1) / 2)
        ax.set_xticklabels([str(t) for t in threads_list])
        ax.yaxis.set_major_formatter(plt.FuncFormatter(lambda v, _: fmt_qps(v)))
        ax.legend()
        fig.savefig(out / f"throughput_bars_{trace}.png")
        plt.close(fig)


# ── Graph 8: Dashboard ──────────────────────────────────────────────────────
def plot_dashboard(agg: list[dict], out: Path):
    for trace, trows in split_by_trace(agg).items():
        fig, axes = plt.subplots(2, 2, figsize=(12, 8))
        by_cache = split_by_cache(trows)

        configs = [
            (axes[0, 0], "throughput", "Throughput (qps)", lambda v, _: fmt_qps(v)),
            (axes[0, 1], "hit_rate", "Hit Rate", lambda v, _: f"{v:.0%}"),
            (axes[1, 0], "avg_latency", "Avg Latency (ns)", None),
            (axes[1, 1], "p99", "P99 Latency (ns)", None),
        ]

        for ax, key, ylabel, fmt in configs:
            for cache, crows in sorted(by_cache.items()):
                crows.sort(key=lambda r: r["threads"])
                xs = [r["threads"] for r in crows]
                ys = [r.get(key, 0) for r in crows]
                s = style_for(cache)
                ax.plot(xs, ys, marker=s["marker"], color=s["color"], label=cache)
            ax.set_xlabel("Threads")
            ax.set_ylabel(ylabel)
            ax.set_xticks(THREAD_COUNTS)
            ax.legend(fontsize=9)
            if fmt:
                ax.yaxis.set_major_formatter(plt.FuncFormatter(fmt))

        fig.suptitle(f"Scaling: FIFO vs SIEVE vs LRU — {trace}", fontsize=14)
        fig.tight_layout()
        fig.savefig(out / f"dashboard_{trace}.png")
        plt.close(fig)


# ── Main ─────────────────────────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(description="Plot scaling comparison.")
    parser.add_argument("--input", nargs="+", default=["results_scaling_sweep.json"],
                        help="One or more JSONL results files to merge")
    parser.add_argument("--output-dir", default="plots_scaling",
                        help="Output directory for plots")
    args = parser.parse_args()

    inputs = [Path(p) for p in args.input]
    out = Path(args.output_dir)
    out.mkdir(parents=True, exist_ok=True)

    records = load_records(inputs)
    agg = aggregate(records)

    print(f"Loaded {len(records)} records → {len(agg)} aggregated rows")
    print(f"Caches: {sorted(set(r['cache'] for r in agg))}")
    print(f"Traces: {sorted(set(r['trace'] for r in agg))}")
    print(f"Threads: {sorted(set(r['threads'] for r in agg))}")

    plot_throughput_vs_threads(agg, out)
    plot_scaling_efficiency(agg, out)
    plot_hit_rate_vs_threads(agg, out)
    plot_avg_latency_vs_threads(agg, out)
    plot_latency_percentiles_vs_threads(agg, out)
    plot_latency_cdf(agg, out)
    plot_throughput_bars(agg, out)
    plot_dashboard(agg, out)

    print(f"\nWrote plots to: {out}/")


if __name__ == "__main__":
    main()
