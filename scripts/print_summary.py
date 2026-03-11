#!/usr/bin/env python3
import argparse
import json
from collections import defaultdict
from pathlib import Path
from statistics import mean
import numpy as np


def load_records(path: Path) -> list[dict]:
    rows: list[dict] = []
    with path.open("r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            rows.append(json.loads(line))
    return rows


def normalize_record(r: dict) -> dict:
    nr = dict(r)
    nr["threads"] = int(nr["threads"])
    nr["batch"] = int(nr.get("batch", 0))
    nr["capacity"] = int(nr.get("capacity", -1))
    return nr


def main() -> None:
    parser = argparse.ArgumentParser(description="Print aggregate benchmark summary.")
    parser.add_argument("--input", default="results.json", help="JSONL results file path")
    args = parser.parse_args()

    rows = [normalize_record(r) for r in load_records(Path(args.input))]
    groups: dict[tuple[str, str, int, int], list[dict]] = defaultdict(list)
    for r in rows:
        groups[
            (r["cache_name"], r["trace_name"], int(r["threads"]), int(r["capacity"]))
        ].append(r)

    print(
        "cache_name,trace_name,threads,capacity,batches,hit_rate_mean,"
        "avg_latency_ns_mean,throughput_qps_mean,p50_ns,p90_ns,p95_ns,p99_ns"
    )
    for (cache, trace, threads, capacity), recs in sorted(groups.items()):
        all_samples = []
        for r in recs:
            all_samples.extend(r.get("samples", []))
        pcts = ""
        if all_samples:
            arr = np.array(all_samples)
            pcts = ",".join(f"{np.percentile(arr, p):.3f}" for p in [50, 90, 95, 99])
        else:
            pcts = ",,,"
        print(
            f"{cache},{trace},{threads},{capacity},{len(recs)},"
            f"{mean(r['hit_rate'] for r in recs):.6f},"
            f"{mean(r['avg_latency_ns'] for r in recs):.3f},"
            f"{mean(r['throughput_qps'] for r in recs):.3f},"
            f"{pcts}"
        )


if __name__ == "__main__":
    main()
