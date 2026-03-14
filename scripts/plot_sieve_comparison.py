#!/usr/bin/env python3
"""
Generate publication-quality graphs comparing SIEVE vs BitSIEVE.
Focused on single-threaded performance across varying cache capacities.

Usage:
    python3 scripts/plot_sieve_comparison.py --input results_capacity_sweep.json
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
    "SIEVE":    {"color": "#1f77b4", "marker": "o"},
    "BitSIEVE": {"color": "#d62728", "marker": "s"},
    # fallbacks
    "SIEVESingle": {"color": "#1f77b4", "marker": "o"},
    "SIEVEBit8":   {"color": "#d62728", "marker": "s"},
}

def style_for(name: str) -> dict:
    return CACHE_STYLES.get(name, {"color": "#333333", "marker": "^"})


# ── Data loading ─────────────────────────────────────────────────────────────
def load_records(path: Path) -> list[dict]:
    records = []
    with path.open() as f:
        for line in f:
            line = line.strip()
            if line:
                records.append(json.loads(line))
    return records


def aggregate(records: list[dict]) -> list[dict]:
    """Aggregate batches → one row per (cache, trace, cap_prop)."""
    groups: dict[tuple, list[dict]] = defaultdict(list)
    for r in records:
        key = (r["cache_name"], r["trace_name"], r["cap_prop"])
        groups[key].append(r)

    out = []
    for (cache, trace, cap_prop), rows in sorted(groups.items()):
        all_samples = []
        for r in rows:
            all_samples.extend(r.get("samples", []))
        pcts = {}
        if all_samples:
            arr = np.array(all_samples)
            for p in [50, 90, 95, 99]:
                pcts[f"p{p}"] = float(np.percentile(arr, p))
        out.append({
            "cache": cache, "trace": trace, "cap_prop": cap_prop,
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


# ── Plot helpers ─────────────────────────────────────────────────────────────
def _lines_vs_capacity(ax, rows_by_cache: dict, ykey: str):
    for cache, crows in sorted(rows_by_cache.items()):
        crows.sort(key=lambda r: r["cap_prop"])
        xs = [r["cap_prop"] * 100 for r in crows]
        ys = [r[ykey] for r in crows]
        s = style_for(cache)
        ax.plot(xs, ys, marker=s["marker"], color=s["color"], label=cache)


def fmt_qps(v: float) -> str:
    if v >= 1e9: return f"{v/1e9:.1f}B"
    if v >= 1e6: return f"{v/1e6:.1f}M"
    if v >= 1e3: return f"{v/1e3:.1f}K"
    return f"{v:.0f}"


# ── Graph 1: Hit rate vs capacity ────────────────────────────────────────────
def plot_hit_rate_vs_capacity(agg: list[dict], out: Path):
    for trace, trows in split_by_trace(agg).items():
        fig, ax = plt.subplots()
        _lines_vs_capacity(ax, split_by_cache(trows), "hit_rate")
        ax.set_xlabel("Cache capacity (% of working set)")
        ax.set_ylabel("Hit rate")
        ax.set_title(f"Hit Rate vs Cache Capacity — {trace}")
        ax.yaxis.set_major_formatter(plt.FuncFormatter(lambda v, _: f"{v:.0%}"))
        ax.legend()
        fig.savefig(out / f"hit_rate_vs_capacity_{trace}.png")
        plt.close(fig)


# ── Graph 2: Throughput vs capacity ──────────────────────────────────────────
def plot_throughput_vs_capacity(agg: list[dict], out: Path):
    for trace, trows in split_by_trace(agg).items():
        fig, ax = plt.subplots()
        _lines_vs_capacity(ax, split_by_cache(trows), "throughput")
        ax.set_xlabel("Cache capacity (% of working set)")
        ax.set_ylabel("Throughput (queries/sec)")
        ax.set_title(f"Throughput vs Cache Capacity — {trace}")
        ax.yaxis.set_major_formatter(plt.FuncFormatter(lambda v, _: fmt_qps(v)))
        ax.legend()
        fig.savefig(out / f"throughput_vs_capacity_{trace}.png")
        plt.close(fig)


# ── Graph 3: Average latency vs capacity ─────────────────────────────────────
def plot_avg_latency_vs_capacity(agg: list[dict], out: Path):
    for trace, trows in split_by_trace(agg).items():
        fig, ax = plt.subplots()
        _lines_vs_capacity(ax, split_by_cache(trows), "avg_latency")
        ax.set_xlabel("Cache capacity (% of working set)")
        ax.set_ylabel("Average latency (ns)")
        ax.set_title(f"Average Latency vs Cache Capacity — {trace}")
        ax.legend()
        fig.savefig(out / f"avg_latency_vs_capacity_{trace}.png")
        plt.close(fig)


# ── Graph 4: Latency percentiles vs capacity ─────────────────────────────────
def plot_latency_percentiles_vs_capacity(agg: list[dict], out: Path):
    pct_keys = ["p50", "p90", "p95", "p99"]
    pct_colors = {"p50": "#2ca02c", "p90": "#ff7f0e", "p95": "#d62728", "p99": "#9467bd"}

    for trace, trows in split_by_trace(agg).items():
        by_cache = split_by_cache(trows)
        fig, axes = plt.subplots(1, len(by_cache), figsize=(7 * len(by_cache), 4.5),
                                  sharey=True, squeeze=False)
        for ax, (cache, crows) in zip(axes[0], sorted(by_cache.items())):
            crows.sort(key=lambda r: r["cap_prop"])
            xs = [r["cap_prop"] * 100 for r in crows]
            for pk in pct_keys:
                if pk in crows[0]:
                    ys = [r[pk] for r in crows]
                    ax.plot(xs, ys, marker="o", color=pct_colors[pk], label=pk)
            ax.set_xlabel("Cache capacity (% of working set)")
            ax.set_title(cache)
            ax.legend(fontsize=9)
        axes[0][0].set_ylabel("Latency (ns)")
        fig.suptitle(f"Latency Percentiles vs Capacity — {trace}", y=1.02)
        fig.savefig(out / f"latency_percentiles_vs_capacity_{trace}.png")
        plt.close(fig)


# ── Graph 5: Latency CDF comparison at specific capacity ─────────────────────
def plot_latency_cdf(agg: list[dict], out: Path):
    """CDF of per-query latency at the median capacity proportion."""
    for trace, trows in split_by_trace(agg).items():
        cap_props = sorted(set(r["cap_prop"] for r in trows))
        # Pick a representative capacity (median of available)
        target_prop = cap_props[len(cap_props) // 2]

        fig, ax = plt.subplots()
        for cache, crows in sorted(split_by_cache(trows).items()):
            row = [r for r in crows if r["cap_prop"] == target_prop]
            if not row or not row[0]["samples"]:
                continue
            samples = np.array(row[0]["samples"])
            sorted_s = np.sort(samples)
            cdf = np.arange(1, len(sorted_s) + 1) / len(sorted_s)
            s = style_for(cache)
            ax.plot(sorted_s, cdf, color=s["color"], label=cache, linewidth=1.5)

        ax.set_xlabel("Latency (ns)")
        ax.set_ylabel("CDF")
        ax.set_title(f"Latency CDF at {target_prop*100:.1f}% capacity — {trace}")
        ax.set_xlim(left=0)
        # Clip x-axis at p99.9 for readability
        ax.set_xlim(right=ax.get_xlim()[1] * 0.5)
        ax.legend()
        fig.savefig(out / f"latency_cdf_{trace}_cap{target_prop}.png")
        plt.close(fig)


# ── Graph 6: Throughput speedup (BitSIEVE / SIEVE) ──────────────────────────
def plot_throughput_speedup(agg: list[dict], out: Path):
    """Bar chart showing relative throughput gain of BitSIEVE over SIEVE."""
    # Find SIEVE-like and BitSIEVE-like names
    cache_names = set(r["cache"] for r in agg)
    sieve_name = next((n for n in cache_names if "Bit" not in n), None)
    bit_name = next((n for n in cache_names if "Bit" in n), None)
    if not sieve_name or not bit_name:
        return

    for trace, trows in split_by_trace(agg).items():
        by_cap = defaultdict(dict)
        for r in trows:
            by_cap[r["cap_prop"]][r["cache"]] = r

        cap_props = sorted(by_cap.keys())
        speedups = []
        for cp in cap_props:
            if sieve_name in by_cap[cp] and bit_name in by_cap[cp]:
                s = by_cap[cp][bit_name]["throughput"] / by_cap[cp][sieve_name]["throughput"]
                speedups.append(s)
            else:
                speedups.append(1.0)

        fig, ax = plt.subplots()
        xs = [cp * 100 for cp in cap_props]
        bars = ax.bar(xs, speedups, width=1.5,
                      color=[style_for(bit_name)["color"] if s >= 1 else style_for(sieve_name)["color"]
                             for s in speedups],
                      alpha=0.8, edgecolor="black", linewidth=0.5)
        ax.axhline(1.0, color="black", linewidth=0.8, linestyle="--")
        ax.set_xlabel("Cache capacity (% of working set)")
        ax.set_ylabel(f"Speedup ({bit_name} / {sieve_name})")
        ax.set_title(f"Throughput Speedup — {trace}")
        # Annotate bars
        for bar, s in zip(bars, speedups):
            ax.text(bar.get_x() + bar.get_width() / 2, bar.get_height() + 0.005,
                    f"{s:.2f}x", ha="center", va="bottom", fontsize=8)
        fig.savefig(out / f"throughput_speedup_{trace}.png")
        plt.close(fig)


# ── Graph 7: Combined metric dashboard ───────────────────────────────────────
def plot_dashboard(agg: list[dict], out: Path):
    """2x2 dashboard: hit rate, throughput, avg latency, p99 latency."""
    for trace, trows in split_by_trace(agg).items():
        fig, axes = plt.subplots(2, 2, figsize=(12, 8))
        by_cache = split_by_cache(trows)

        configs = [
            (axes[0, 0], "hit_rate", "Hit Rate", lambda v, _: f"{v:.0%}"),
            (axes[0, 1], "throughput", "Throughput (qps)", lambda v, _: fmt_qps(v)),
            (axes[1, 0], "avg_latency", "Avg Latency (ns)", None),
            (axes[1, 1], "p99", "P99 Latency (ns)", None),
        ]

        for ax, key, ylabel, fmt in configs:
            for cache, crows in sorted(by_cache.items()):
                crows.sort(key=lambda r: r["cap_prop"])
                xs = [r["cap_prop"] * 100 for r in crows]
                ys = [r.get(key, 0) for r in crows]
                s = style_for(cache)
                ax.plot(xs, ys, marker=s["marker"], color=s["color"], label=cache)
            ax.set_xlabel("Capacity (% of working set)")
            ax.set_ylabel(ylabel)
            ax.legend(fontsize=9)
            if fmt:
                ax.yaxis.set_major_formatter(plt.FuncFormatter(fmt))

        fig.suptitle(f"SIEVE vs BitSIEVE — {trace}", fontsize=14)
        fig.tight_layout()
        fig.savefig(out / f"dashboard_{trace}.png")
        plt.close(fig)


# ── Main ─────────────────────────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(description="Plot SIEVE vs BitSIEVE comparison.")
    parser.add_argument("--input", default="results_capacity_sweep.json",
                        help="JSONL results file")
    parser.add_argument("--output-dir", default="plots_sieve_comparison",
                        help="Output directory for plots")
    args = parser.parse_args()

    inp = Path(args.input)
    out = Path(args.output_dir)
    out.mkdir(parents=True, exist_ok=True)

    records = load_records(inp)
    agg = aggregate(records)

    print(f"Loaded {len(records)} records → {len(agg)} aggregated rows")
    print(f"Caches: {sorted(set(r['cache'] for r in agg))}")
    print(f"Traces: {sorted(set(r['trace'] for r in agg))}")
    print(f"Capacities: {sorted(set(r['cap_prop'] for r in agg))}")

    plot_hit_rate_vs_capacity(agg, out)
    plot_throughput_vs_capacity(agg, out)
    plot_avg_latency_vs_capacity(agg, out)
    plot_latency_percentiles_vs_capacity(agg, out)
    plot_latency_cdf(agg, out)
    plot_throughput_speedup(agg, out)
    plot_dashboard(agg, out)

    print(f"\nWrote plots to: {out}/")


if __name__ == "__main__":
    main()
