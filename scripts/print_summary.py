#!/usr/bin/env python3
import argparse
import json
from collections import defaultdict
from pathlib import Path
from statistics import mean


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
        "cache_name,trace_name,threads,capacity,batches,hit_rate_mean,avg_latency_ns_mean,throughput_qps_mean"
    )
    for (cache, trace, threads, capacity), recs in sorted(groups.items()):
        print(
            f"{cache},{trace},{threads},{capacity},{len(recs)},"
            f"{mean(r['hit_rate'] for r in recs):.6f},"
            f"{mean(r['avg_latency_ns'] for r in recs):.3f},"
            f"{mean(r['throughput_qps'] for r in recs):.3f}"
        )


if __name__ == "__main__":
    main()
