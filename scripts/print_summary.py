#!/usr/bin/env python3
"""Print a nicely formatted textual summary of benchmark results."""
import argparse
import json
import sys
from collections import defaultdict
from pathlib import Path
from statistics import mean, stdev
import numpy as np


def load_records(path: Path) -> list[dict]:
    rows: list[dict] = []
    with path.open("r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if line:
                rows.append(json.loads(line))
    return rows


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


def fmt_pct(v: float) -> str:
    return f"{v * 100:.2f}%"


def print_table(headers: list[str], rows: list[list[str]], alignments: list[str] | None = None):
    """Print an ASCII table. alignments: 'l' or 'r' per column."""
    if not rows:
        return
    col_widths = [len(h) for h in headers]
    for row in rows:
        for i, cell in enumerate(row):
            col_widths[i] = max(col_widths[i], len(cell))

    if alignments is None:
        alignments = ["l"] * len(headers)

    def fmt_cell(val: str, width: int, align: str) -> str:
        return val.ljust(width) if align == "l" else val.rjust(width)

    sep = "+-" + "-+-".join("-" * w for w in col_widths) + "-+"
    header_line = "| " + " | ".join(
        fmt_cell(h, col_widths[i], alignments[i]) for i, h in enumerate(headers)
    ) + " |"

    print(sep)
    print(header_line)
    print(sep)
    for row in rows:
        line = "| " + " | ".join(
            fmt_cell(row[i], col_widths[i], alignments[i]) for i in range(len(headers))
        ) + " |"
        print(line)
    print(sep)


def main() -> None:
    parser = argparse.ArgumentParser(description="Print benchmark summary.")
    parser.add_argument("--input", default="results.json", help="JSONL results file")
    parser.add_argument("--batch-detail", action="store_true",
                        help="Show per-batch throughput breakdown")
    parser.add_argument("--cache", help="Filter to specific cache name")
    parser.add_argument("--trace", help="Filter to specific trace name")
    args = parser.parse_args()

    records = load_records(Path(args.input))
    if not records:
        print("No records found.", file=sys.stderr)
        sys.exit(1)

    if args.cache:
        records = [r for r in records if r["cache_name"] == args.cache]
    if args.trace:
        records = [r for r in records if r["trace_name"] == args.trace]

    # Group by (trace, cache, threads, cap_prop, scale_policy)
    groups: dict[tuple, list[dict]] = defaultdict(list)
    for r in records:
        key = (r["trace_name"], r["cache_name"], int(r["threads"]),
               r.get("cap_prop", -1), r.get("scale_policy", "unknown"))
        groups[key].append(r)

    # --- Aggregate summary ---
    print("\n=== AGGREGATE SUMMARY ===\n")
    headers = ["trace", "cache", "thr", "cap_prop", "batches",
               "hit_rate", "throughput", "avg_lat",
               "p50", "p90", "p95", "p99"]
    aligns = ["l", "l", "r", "r", "r", "r", "r", "r", "r", "r", "r", "r"]
    table_rows = []

    for (trace, cache, threads, cap_prop, _sp), recs in sorted(groups.items()):
        recs.sort(key=lambda x: int(x.get("batch", 0)))
        throughputs = [r["throughput_qps"] for r in recs]
        hit_rates = [r["hit_rate"] for r in recs]
        avg_lats = [r["avg_latency_ns"] for r in recs]

        all_samples = []
        for r in recs:
            all_samples.extend(r.get("samples", []))

        pcts = {}
        if all_samples:
            arr = np.array(all_samples)
            for p in [50, 90, 95, 99]:
                pcts[p] = float(np.percentile(arr, p))

        row = [
            trace, cache, str(threads), f"{cap_prop:.1f}", str(len(recs)),
            fmt_pct(mean(hit_rates)),
            fmt_qps(mean(throughputs)),
            fmt_ns(mean(avg_lats)),
            fmt_ns(pcts[50]) if 50 in pcts else "-",
            fmt_ns(pcts[90]) if 90 in pcts else "-",
            fmt_ns(pcts[95]) if 95 in pcts else "-",
            fmt_ns(pcts[99]) if 99 in pcts else "-",
        ]
        table_rows.append(row)

    print_table(headers, table_rows, aligns)

    # --- Throughput stability ---
    print("\n=== THROUGHPUT STABILITY (across batches) ===\n")
    headers2 = ["trace", "cache", "min_qps", "max_qps", "mean_qps", "stddev", "cv%"]
    aligns2 = ["l", "l", "r", "r", "r", "r", "r"]
    stab_rows = []

    for (trace, cache, threads, cap_prop, _sp), recs in sorted(groups.items()):
        throughputs = [r["throughput_qps"] for r in recs]
        m = mean(throughputs)
        sd = stdev(throughputs) if len(throughputs) > 1 else 0
        cv = (sd / m * 100) if m > 0 else 0
        stab_rows.append([
            trace, cache,
            fmt_qps(min(throughputs)), fmt_qps(max(throughputs)),
            fmt_qps(m), fmt_qps(sd), f"{cv:.1f}%",
        ])

    print_table(headers2, stab_rows, aligns2)

    # --- Per-batch detail ---
    if args.batch_detail:
        for (trace, cache, threads, cap_prop, _sp), recs in sorted(groups.items()):
            recs.sort(key=lambda x: int(x.get("batch", 0)))
            print(f"\n=== BATCH DETAIL: {cache} on {trace} (threads={threads}, cap_prop={cap_prop}) ===\n")
            headers3 = ["batch", "hit_rate", "throughput", "avg_lat"]
            aligns3 = ["r", "r", "r", "r"]
            batch_rows = []
            for r in recs:
                batch_rows.append([
                    str(int(r.get("batch", 0))),
                    fmt_pct(r["hit_rate"]),
                    fmt_qps(r["throughput_qps"]),
                    fmt_ns(r["avg_latency_ns"]),
                ])
            print_table(headers3, batch_rows, aligns3)


if __name__ == "__main__":
    main()
