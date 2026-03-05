#!/usr/bin/env python3
import argparse
import json
from collections import defaultdict
from pathlib import Path
from statistics import mean

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
        out.append(nr)
    return out


def aggregate_by_cache_trace_thread_capacity(records: list[dict]) -> list[dict]:
    groups: dict[tuple[str, str, int, int], list[dict]] = defaultdict(list)
    for r in records:
        key = (
            r["cache_name"],
            r["trace_name"],
            int(r["threads"]),
            int(r["capacity"]),
        )
        groups[key].append(r)

    out: list[dict] = []
    for (cache, trace, threads, capacity), rows in sorted(groups.items()):
        out.append(
            {
                "cache_name": cache,
                "trace_name": trace,
                "threads": threads,
                "capacity": capacity,
                "batches": len(rows),
                "hit_rate_mean": mean(r["hit_rate"] for r in rows),
                "avg_latency_ns_mean": mean(r["avg_latency_ns"] for r in rows),
                "throughput_qps_mean": mean(r["throughput_qps"] for r in rows),
                "cost_ns_mean": mean(r["cost_ns"] for r in rows),
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
        "cost_ns_mean",
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
                        f"{r['cost_ns_mean']:.8f}",
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
        by_capacity: dict[int, list[dict]] = defaultdict(list)
        for r in trace_rows:
            by_capacity[int(r["capacity"])].append(r)

        for capacity, cap_rows in sorted(by_capacity.items()):
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
                f"{trace}: {ylabel} vs threads (cap={capacity}, mean over batches)"
            )
            plt.xlabel("threads")
            plt.ylabel(ylabel)
            plt.grid(True, alpha=0.3)
            plt.legend()
            plt.tight_layout()
            plt.savefig(
                output_dir / f"agg_{trace}_cap{capacity}_vs_threads_{metric}.png",
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
                cache_rows.sort(key=lambda x: x["capacity"])
                xs = [r["capacity"] for r in cache_rows]
                ys = [r[metric] for r in cache_rows]
                plt.plot(xs, ys, marker="o", label=cache)

            plt.title(
                f"{trace}: {ylabel} vs capacity (threads={threads}, mean over batches)"
            )
            plt.xlabel("capacity")
            plt.ylabel(ylabel)
            plt.grid(True, alpha=0.3)
            plt.legend()
            plt.tight_layout()
            plt.savefig(
                output_dir / f"agg_{trace}_thr{threads}_vs_capacity_{metric}.png",
                dpi=140,
            )
            plt.close()


def plot_batch_series(records: list[dict], output_dir: Path, metric: str, ylabel: str) -> None:
    by_cache_trace_capacity: dict[tuple[str, str, int], list[dict]] = defaultdict(list)
    for r in records:
        by_cache_trace_capacity[
            (r["cache_name"], r["trace_name"], int(r["capacity"]))
        ].append(r)

    for (cache, trace, capacity), rows in sorted(by_cache_trace_capacity.items()):
        plt.figure(figsize=(8, 5))
        by_thread: dict[int, list[dict]] = defaultdict(list)
        for r in rows:
            by_thread[int(r["threads"])].append(r)

        for threads, trows in sorted(by_thread.items()):
            trows.sort(key=lambda x: int(x["batch"]))
            xs = [int(r["batch"]) for r in trows]
            ys = [r[metric] for r in trows]
            plt.plot(xs, ys, marker=".", label=f"{threads} threads")

        plt.title(f"{cache} on {trace} (cap={capacity}): {ylabel} by batch")
        plt.xlabel("batch")
        plt.ylabel(ylabel)
        plt.grid(True, alpha=0.3)
        plt.legend()
        plt.tight_layout()
        plt.savefig(output_dir / f"batch_{cache}_{trace}_cap{capacity}_{metric}.png", dpi=140)
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
    summary = aggregate_by_cache_trace_thread_capacity(records)
    write_summary_csv(summary, output_dir / "aggregate_summary.csv")

    plot_aggregate_vs_threads(
        summary, output_dir, metric="throughput_qps_mean", ylabel="throughput (qps)"
    )
    plot_aggregate_vs_threads(
        summary, output_dir, metric="avg_latency_ns_mean", ylabel="avg latency (ns)"
    )
    plot_aggregate_vs_threads(summary, output_dir, metric="cost_ns_mean", ylabel="cost (ns)")
    plot_aggregate_vs_threads(summary, output_dir, metric="hit_rate_mean", ylabel="hit rate")

    plot_aggregate_vs_capacity(
        summary, output_dir, metric="throughput_qps_mean", ylabel="throughput (qps)"
    )
    plot_aggregate_vs_capacity(
        summary, output_dir, metric="avg_latency_ns_mean", ylabel="avg latency (ns)"
    )
    plot_aggregate_vs_capacity(
        summary, output_dir, metric="cost_ns_mean", ylabel="cost (ns)"
    )
    plot_aggregate_vs_capacity(summary, output_dir, metric="hit_rate_mean", ylabel="hit rate")

    plot_batch_series(
        records, output_dir, metric="throughput_qps", ylabel="throughput (qps)"
    )
    plot_batch_series(
        records, output_dir, metric="avg_latency_ns", ylabel="avg latency (ns)"
    )
    plot_batch_series(records, output_dir, metric="cost_ns", ylabel="cost (ns)")

    print(f"Wrote plots and summary to: {output_dir}")


if __name__ == "__main__":
    main()
