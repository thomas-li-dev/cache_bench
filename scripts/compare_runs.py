#!/usr/bin/env python3
"""Compare two benchmark result files side-by-side. Useful for A/B testing cache implementations."""
import argparse
import json
import sys
from collections import defaultdict
from pathlib import Path
from statistics import mean
import numpy as np


def load_records(path: Path) -> list[dict]:
    rows: list[dict] = []
    with path.open("r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if line:
                rows.append(json.loads(line))
    return rows


def aggregate(records: list[dict]) -> dict[tuple, dict]:
    groups: dict[tuple, list[dict]] = defaultdict(list)
    for r in records:
        key = (r["trace_name"], r["cache_name"], int(r["threads"]),
               r.get("cap_prop", -1))
        groups[key].append(r)

    out = {}
    for key, recs in groups.items():
        all_samples = []
        for r in recs:
            all_samples.extend(r.get("samples", []))
        pcts = {}
        if all_samples:
            arr = np.array(all_samples)
            for p in [50, 90, 95, 99]:
                pcts[f"p{p}"] = float(np.percentile(arr, p))
        out[key] = {
            "throughput": mean(r["throughput_qps"] for r in recs),
            "avg_lat": mean(r["avg_latency_ns"] for r in recs),
            "hit_rate": mean(r["hit_rate"] for r in recs),
            "batches": len(recs),
            **pcts,
        }
    return out


def fmt_qps(v: float) -> str:
    if v >= 1e9:
        return f"{v / 1e9:.2f}B"
    if v >= 1e6:
        return f"{v / 1e6:.2f}M"
    if v >= 1e3:
        return f"{v / 1e3:.2f}K"
    return f"{v:.0f}"


def fmt_ns(v: float) -> str:
    if v >= 1e6:
        return f"{v / 1e6:.2f}ms"
    if v >= 1e3:
        return f"{v / 1e3:.2f}us"
    return f"{v:.1f}ns"


def fmt_delta(old: float, new: float) -> str:
    if old == 0:
        return "N/A"
    pct = (new - old) / old * 100
    sign = "+" if pct >= 0 else ""
    return f"{sign}{pct:.1f}%"


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Compare two benchmark runs (A=baseline, B=experiment).")
    parser.add_argument("baseline", help="Baseline results.json (A)")
    parser.add_argument("experiment", help="Experiment results.json (B)")
    parser.add_argument("--cache", help="Filter to specific cache name")
    parser.add_argument("--trace", help="Filter to specific trace name")
    args = parser.parse_args()

    a_recs = load_records(Path(args.baseline))
    b_recs = load_records(Path(args.experiment))

    if args.cache:
        a_recs = [r for r in a_recs if r["cache_name"] == args.cache]
        b_recs = [r for r in b_recs if r["cache_name"] == args.cache]
    if args.trace:
        a_recs = [r for r in a_recs if r["trace_name"] == args.trace]
        b_recs = [r for r in b_recs if r["trace_name"] == args.trace]

    a_agg = aggregate(a_recs)
    b_agg = aggregate(b_recs)

    all_keys = sorted(set(a_agg.keys()) | set(b_agg.keys()))
    if not all_keys:
        print("No matching configurations found.", file=sys.stderr)
        sys.exit(1)

    print(f"\nBaseline:   {args.baseline}")
    print(f"Experiment: {args.experiment}")
    print()

    for key in all_keys:
        trace, cache, threads, cap_prop = key
        print(f"--- {cache} | {trace} | threads={threads} | cap_prop={cap_prop} ---")

        a = a_agg.get(key)
        b = b_agg.get(key)

        if a is None:
            print("  (only in experiment)\n")
            continue
        if b is None:
            print("  (only in baseline)\n")
            continue

        # Throughput: higher is better
        print(f"  throughput:  {fmt_qps(a['throughput']):>10}  ->  {fmt_qps(b['throughput']):>10}  ({fmt_delta(a['throughput'], b['throughput'])})")

        # Avg latency: lower is better
        lat_delta = fmt_delta(a['avg_lat'], b['avg_lat'])
        print(f"  avg_lat:     {fmt_ns(a['avg_lat']):>10}  ->  {fmt_ns(b['avg_lat']):>10}  ({lat_delta})")

        # Hit rate
        print(f"  hit_rate:    {a['hit_rate']*100:>9.2f}%  ->  {b['hit_rate']*100:>9.2f}%")

        # Latency percentiles
        for p in ["p50", "p90", "p95", "p99"]:
            if p in a and p in b:
                print(f"  {p}:         {fmt_ns(a[p]):>10}  ->  {fmt_ns(b[p]):>10}  ({fmt_delta(a[p], b[p])})")

        print()


if __name__ == "__main__":
    main()
