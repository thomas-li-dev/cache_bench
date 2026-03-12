#!/usr/bin/env python3
import argparse
import json
from collections import defaultdict
from pathlib import Path
from statistics import mean
import numpy as np

try:
    import matplotlib.pyplot as plt
except ImportError as e:
    raise SystemExit(
        "matplotlib is required. Install with: pip install matplotlib"
    ) from e


def load_records(path: Path) -> list[dict]:
    records: list[dict] = []
    with path.open("r", encoding="utf-8") as f:
        for line_no, line in enumerate(f, start=1):
            line = line.strip()
            if not line:
                continue
            try:
                rec = json.loads(line)
            except json.JSONDecodeError as e:
                raise ValueError(f"Invalid JSON on line {line_no}: {e}") from e
            records.append(rec)
    if not records:
        raise ValueError(f"No records found in {path}")
    return records


def normalize_records(records: list[dict]) -> list[dict]:
    out: list[dict] = []
    for r in records:
        nr = dict(r)
        nr["threads"] = int(nr["threads"])
        nr["batch"] = int(nr.get("batch", 0))
        nr["capacity"] = int(nr.get("capacity", -1))
        nr["cap_prop"] = float(nr.get("cap_prop", -1))
        out.append(nr)
    return out


def aggregate_by_cache_trace_thread_capprop(records: list[dict]) -> list[dict]:
    groups: dict[tuple[str, str, int, float], list[dict]] = defaultdict(list)
    for r in records:
        key = (
            r["cache_name"],
            r["trace_name"],
            int(r["threads"]),
            r["cap_prop"],
        )
        groups[key].append(r)

    out: list[dict] = []
    for (cache, trace, threads, cap_prop), rows in sorted(groups.items()):
        all_samples = []
        for r in rows:
            all_samples.extend(r.get("samples", []))
        percentiles = {}
        if all_samples:
            arr = np.array(all_samples)
            for p in [50, 90, 95, 99]:
                percentiles[f"p{p}_latency_ns"] = float(np.percentile(arr, p))
        out.append(
            {
                "cache_name": cache,
                "trace_name": trace,
                "threads": threads,
                "cap_prop": cap_prop,
                "capacity": int(mean(r["capacity"] for r in rows)),
                "batches": len(rows),
                "hit_rate_mean": mean(r["hit_rate"] for r in rows),
                "avg_latency_ns_mean": mean(r["avg_latency_ns"] for r in rows),
                "throughput_qps_mean": mean(r["throughput_qps"] for r in rows),
                **percentiles,
            }
        )
    return out


def write_summary_csv(rows: list[dict], out_path: Path) -> None:
    headers = [
        "cache_name",
        "trace_name",
        "threads",
        "capacity",
        "batches",
        "hit_rate_mean",
        "avg_latency_ns_mean",
        "throughput_qps_mean",
        "p50_latency_ns",
        "p90_latency_ns",
        "p95_latency_ns",
        "p99_latency_ns",
    ]
    with out_path.open("w", encoding="utf-8") as f:
        f.write(",".join(headers) + "\n")
        for r in rows:
            f.write(
                ",".join(
                    [
                        str(r["cache_name"]),
                        str(r["trace_name"]),
                        str(r["threads"]),
                        str(r["capacity"]),
                        str(r["batches"]),
                        f"{r['hit_rate_mean']:.8f}",
                        f"{r['avg_latency_ns_mean']:.8f}",
                        f"{r['throughput_qps_mean']:.8f}",
                        f"{r.get('p50_latency_ns', ''):.8f}" if 'p50_latency_ns' in r else "",
                        f"{r.get('p90_latency_ns', ''):.8f}" if 'p90_latency_ns' in r else "",
                        f"{r.get('p95_latency_ns', ''):.8f}" if 'p95_latency_ns' in r else "",
                        f"{r.get('p99_latency_ns', ''):.8f}" if 'p99_latency_ns' in r else "",
                    ]
                )
                + "\n"
            )


def plot_aggregate_vs_threads(
    rows: list[dict], output_dir: Path, metric: str, ylabel: str
) -> None:
    by_trace: dict[str, list[dict]] = defaultdict(list)
    for r in rows:
        by_trace[r["trace_name"]].append(r)

    for trace, trace_rows in by_trace.items():
        by_cap_prop: dict[float, list[dict]] = defaultdict(list)
        for r in trace_rows:
            by_cap_prop[r["cap_prop"]].append(r)

        for cap_prop, cap_rows in sorted(by_cap_prop.items()):
            plt.figure(figsize=(8, 5))
            by_cache: dict[str, list[dict]] = defaultdict(list)
            for r in cap_rows:
                by_cache[r["cache_name"]].append(r)

            for cache, cache_rows in sorted(by_cache.items()):
                cache_rows.sort(key=lambda x: x["threads"])
                xs = [r["threads"] for r in cache_rows]
                ys = [r[metric] for r in cache_rows]
                plt.plot(xs, ys, marker="o", label=cache)

            plt.title(
                f"{trace}: {ylabel} vs threads (cap_prop={cap_prop}, mean over batches)"
            )
            plt.xlabel("threads")
            plt.ylabel(ylabel)
            plt.grid(True, alpha=0.3)
            plt.legend()
            plt.tight_layout()
            plt.savefig(
                output_dir / f"agg_{trace}_prop{cap_prop}_vs_threads_{metric}.png",
                dpi=140,
            )
            plt.close()


def plot_aggregate_vs_capacity(
    rows: list[dict], output_dir: Path, metric: str, ylabel: str
) -> None:
    by_trace: dict[str, list[dict]] = defaultdict(list)
    for r in rows:
        by_trace[r["trace_name"]].append(r)

    for trace, trace_rows in by_trace.items():
        by_threads: dict[int, list[dict]] = defaultdict(list)
        for r in trace_rows:
            by_threads[int(r["threads"])].append(r)

        for threads, t_rows in sorted(by_threads.items()):
            plt.figure(figsize=(8, 5))
            by_cache: dict[str, list[dict]] = defaultdict(list)
            for r in t_rows:
                by_cache[r["cache_name"]].append(r)

            for cache, cache_rows in sorted(by_cache.items()):
                cache_rows.sort(key=lambda x: x["cap_prop"])
                xs = [r["cap_prop"] for r in cache_rows]
                ys = [r[metric] for r in cache_rows]
                plt.plot(xs, ys, marker="o", label=cache)

            plt.title(
                f"{trace}: {ylabel} vs cap_prop (threads={threads}, mean over batches)"
            )
            plt.xlabel("cap_prop (fraction of working set)")
            plt.ylabel(ylabel)
            plt.grid(True, alpha=0.3)
            plt.legend()
            plt.tight_layout()
            plt.savefig(
                output_dir / f"agg_{trace}_thr{threads}_vs_capprop_{metric}.png",
                dpi=140,
            )
            plt.close()


def plot_batch_series(records: list[dict], output_dir: Path, metric: str, ylabel: str) -> None:
    by_cache_trace_capprop: dict[tuple[str, str, float], list[dict]] = defaultdict(list)
    for r in records:
        by_cache_trace_capprop[
            (r["cache_name"], r["trace_name"], r["cap_prop"])
        ].append(r)

    for (cache, trace, cap_prop), rows in sorted(by_cache_trace_capprop.items()):
        plt.figure(figsize=(8, 5))
        by_thread: dict[int, list[dict]] = defaultdict(list)
        for r in rows:
            by_thread[int(r["threads"])].append(r)

        for threads, trows in sorted(by_thread.items()):
            trows.sort(key=lambda x: int(x["batch"]))
            xs = [int(r["batch"]) for r in trows]
            ys = [r[metric] for r in trows]
            plt.plot(xs, ys, marker=".", label=f"{threads} threads")

        plt.title(f"{cache} on {trace} (cap_prop={cap_prop}): {ylabel} by batch")
        plt.xlabel("batch")
        plt.ylabel(ylabel)
        plt.grid(True, alpha=0.3)
        plt.legend()
        plt.tight_layout()
        plt.savefig(output_dir / f"batch_{cache}_{trace}_prop{cap_prop}_{metric}.png", dpi=140)
        plt.close()


def plot_latency_percentiles_vs_threads(
    rows: list[dict], output_dir: Path
) -> None:
    percentile_keys = ["p50_latency_ns", "p90_latency_ns", "p95_latency_ns", "p99_latency_ns"]
    by_trace: dict[str, list[dict]] = defaultdict(list)
    for r in rows:
        by_trace[r["trace_name"]].append(r)

    for trace, trace_rows in by_trace.items():
        by_cap_prop: dict[float, list[dict]] = defaultdict(list)
        for r in trace_rows:
            by_cap_prop[r["cap_prop"]].append(r)

        for cap_prop, cap_rows in sorted(by_cap_prop.items()):
            plt.figure(figsize=(10, 6))
            by_cache: dict[str, list[dict]] = defaultdict(list)
            for r in cap_rows:
                if "p50_latency_ns" in r:
                    by_cache[r["cache_name"]].append(r)

            for cache, cache_rows in sorted(by_cache.items()):
                cache_rows.sort(key=lambda x: x["threads"])
                xs = [r["threads"] for r in cache_rows]
                for pk in percentile_keys:
                    ys = [r[pk] for r in cache_rows]
                    plt.plot(xs, ys, marker="o", label=f"{cache} {pk.split('_')[0]}")

            plt.title(f"{trace}: latency percentiles vs threads (cap_prop={cap_prop})")
            plt.xlabel("threads")
            plt.ylabel("latency (ns)")
            plt.grid(True, alpha=0.3)
            plt.legend(fontsize="small")
            plt.tight_layout()
            plt.savefig(
                output_dir / f"latency_pct_{trace}_prop{cap_prop}_vs_threads.png",
                dpi=140,
            )
            plt.close()


def plot_latency_percentiles_vs_capacity(
    rows: list[dict], output_dir: Path
) -> None:
    percentile_keys = ["p50_latency_ns", "p90_latency_ns", "p95_latency_ns", "p99_latency_ns"]
    by_trace: dict[str, list[dict]] = defaultdict(list)
    for r in rows:
        by_trace[r["trace_name"]].append(r)

    for trace, trace_rows in by_trace.items():
        by_threads: dict[int, list[dict]] = defaultdict(list)
        for r in trace_rows:
            by_threads[int(r["threads"])].append(r)

        for threads, t_rows in sorted(by_threads.items()):
            plt.figure(figsize=(10, 6))
            by_cache: dict[str, list[dict]] = defaultdict(list)
            for r in t_rows:
                if "p50_latency_ns" in r:
                    by_cache[r["cache_name"]].append(r)

            for cache, cache_rows in sorted(by_cache.items()):
                cache_rows.sort(key=lambda x: x["cap_prop"])
                xs = [r["cap_prop"] for r in cache_rows]
                for pk in percentile_keys:
                    ys = [r[pk] for r in cache_rows]
                    plt.plot(xs, ys, marker="o", label=f"{cache} {pk.split('_')[0]}")

            plt.title(f"{trace}: latency percentiles vs cap_prop (threads={threads})")
            plt.xlabel("cap_prop (fraction of working set)")
            plt.ylabel("latency (ns)")
            plt.grid(True, alpha=0.3)
            plt.legend(fontsize="small")
            plt.tight_layout()
            plt.savefig(
                output_dir / f"latency_pct_{trace}_thr{threads}_vs_capprop.png",
                dpi=140,
            )
            plt.close()


def plot_latency_boxplots(records: list[dict], output_dir: Path) -> None:
    by_trace: dict[str, list[dict]] = defaultdict(list)
    for r in records:
        by_trace[r["trace_name"]].append(r)

    for trace, trace_rows in by_trace.items():
        by_threads_capprop: dict[tuple[int, float], list[dict]] = defaultdict(list)
        for r in trace_rows:
            by_threads_capprop[(int(r["threads"]), r["cap_prop"])].append(r)

        for (threads, cap_prop), t_rows in sorted(by_threads_capprop.items()):
            by_cache: dict[str, list[float]] = defaultdict(list)
            for r in t_rows:
                for s in r.get("samples", []):
                    by_cache[r["cache_name"]].append(s)

            if not by_cache:
                continue

            labels = sorted(by_cache.keys())
            data = [by_cache[k] for k in labels]

            plt.figure(figsize=(max(8, len(labels) * 1.2), 6))
            plt.boxplot(data, labels=labels, showfliers=False)
            plt.title(f"{trace}: latency distribution (threads={threads}, cap_prop={cap_prop})")
            plt.ylabel("latency (ns)")
            plt.xticks(rotation=45, ha="right", fontsize="small")
            plt.grid(True, alpha=0.3, axis="y")
            plt.tight_layout()
            plt.savefig(
                output_dir / f"latency_box_{trace}_thr{threads}_prop{cap_prop}.png",
                dpi=140,
            )
            plt.close()


def main() -> None:
    parser = argparse.ArgumentParser(description="Plot cache benchmark JSONL results.")
    parser.add_argument(
        "--input",
        default="results.json",
        help="Path to JSONL results file (default: results.json)",
    )
    parser.add_argument(
        "--output-dir",
        default="plots",
        help="Directory to write plots and summary CSV (default: plots)",
    )
    args = parser.parse_args()

    input_path = Path(args.input)
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    records = normalize_records(load_records(input_path))
    summary = aggregate_by_cache_trace_thread_capprop(records)
    write_summary_csv(summary, output_dir / "aggregate_summary.csv")

    plot_aggregate_vs_threads(
        summary, output_dir, metric="throughput_qps_mean", ylabel="throughput (qps)"
    )
    plot_aggregate_vs_threads(
        summary, output_dir, metric="avg_latency_ns_mean", ylabel="avg latency (ns)"
    )
    plot_aggregate_vs_threads(summary, output_dir, metric="hit_rate_mean", ylabel="hit rate")

    plot_aggregate_vs_capacity(
        summary, output_dir, metric="throughput_qps_mean", ylabel="throughput (qps)"
    )
    plot_aggregate_vs_capacity(
        summary, output_dir, metric="avg_latency_ns_mean", ylabel="avg latency (ns)"
    )
    plot_aggregate_vs_capacity(summary, output_dir, metric="hit_rate_mean", ylabel="hit rate")

    plot_batch_series(
        records, output_dir, metric="throughput_qps", ylabel="throughput (qps)"
    )
    plot_batch_series(
        records, output_dir, metric="avg_latency_ns", ylabel="avg latency (ns)"
    )

    plot_latency_percentiles_vs_threads(summary, output_dir)
    plot_latency_percentiles_vs_capacity(summary, output_dir)
    plot_latency_boxplots(records, output_dir)

    print(f"Wrote plots and summary to: {output_dir}")


if __name__ == "__main__":
    main()
