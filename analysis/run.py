from __future__ import annotations

import argparse
import json
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from cache_algos import CACHE_ALGORITHMS
from trace_parser import read_requests, trace_name

from tqdm import tqdm

SCHEMA_VERSION = 1

def _now_iso() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat()

def _merge_algos(existing: list[str], new_algos: list[str]) -> list[str]:
    merged: list[str] = []
    seen: set[str] = set()

    for name in existing + new_algos:
        if name in seen:
            continue
        seen.add(name)
        merged.append(name)

    if "fifo" in merged:
        merged = ["fifo"] + [a for a in merged if a != "fifo"]
    return merged

def compute_footprint(tracepath: str) -> int:
    unique: set[int] = set()
    for _, obj_id, _ in read_requests(tracepath):
        unique.add(obj_id)
    return len(unique)

def simulate_trace(
    tracepath: str,
    algo_sizes: dict[str, list[int]],
) -> dict[str, dict[int, float]]:
    all_caches: dict[str, list[tuple[int, Any]]] = {}
    for algo, sizes in algo_sizes.items():
        cls = CACHE_ALGORITHMS.get(algo)
        if cls is None:
            sys.exit(
                f"Unknown algorithm '{algo}'. Supported: {sorted(CACHE_ALGORITHMS)}"
            )
        all_caches[algo] = [(sz, cls(sz)) for sz in sizes]

    get_calls: list[Any] = []
    for cache_list in all_caches.values():
        for _, cache in cache_list:
            get_calls.append(cache.get)

    # Bind once outside loop to reduce attribute lookups in the hot path.
    for _, obj_id, _ in read_requests(tracepath):
        for get in get_calls:
            get(obj_id)

    return {
        algo: {sz: cache.miss_ratio for sz, cache in cache_list}
        for algo, cache_list in all_caches.items()
    }

def load_results(path: Path) -> dict[str, Any]:
    if path.exists():
        with open(path) as f:
            return json.load(f)
    return {}

def save_results(data: dict[str, Any], path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w") as f:
        json.dump(data, f, indent=2, sort_keys=True)

def _collect_local_trace_sources(trace_dir: Path, trace_globs: list[str]) -> list[str]:
    traces_set: set[Path] = set()
    for pat in trace_globs:
        traces_set.update(trace_dir.glob(pat))
    return [str(p) for p in sorted(traces_set)]

def run(
    trace_sources: list[str],
    algos: list[str],
    large_frac: float,
    small_frac: float,
    output: Path,
    resume: bool,
    trace_source_label: str,
) -> dict[str, Any]:
    if not trace_sources:
        sys.exit("No trace sources provided")

    all_algos = ["fifo"] + [a for a in algos if a != "fifo"]

    existing: dict[str, Any] = load_results(output) if resume else {}
    data: dict[str, Any] = existing if existing else {}

    # Initialize structure (and keep it stable across resumes)
    data.setdefault(
        "meta",
        {
            "schema_version": SCHEMA_VERSION,
            "generated_at_utc": _now_iso(),
            "trace_source": trace_source_label,
            "large_frac": large_frac,
            "small_frac": small_frac,
            "algos": all_algos,
        },
    )
    data["meta"]["generated_at_utc"] = _now_iso()
    prev_algos = data["meta"].get("algos", [])
    if not isinstance(prev_algos, list):
        prev_algos = []
    data["meta"]["algos"] = _merge_algos(prev_algos, all_algos)
    data["meta"]["large_frac"] = large_frac
    data["meta"]["small_frac"] = small_frac
    data["meta"]["trace_source"] = trace_source_label

    traces_out = data.setdefault("traces", {})

    progress = tqdm(trace_sources, unit="trace", desc="Traces")
    for trace in progress:
        key = trace_name(trace)
        progress.set_postfix_str(f"{key} | start")
        entry = traces_out.setdefault(
            key,
            {
                "path": trace,
                "footprint": None,
                "large_size": None,
                "small_size": None,
                "miss_ratios": {},
            },
        )
        entry["path"] = trace

        if not entry.get("footprint"):
            progress.set_postfix_str(f"{key} | footprint")
            progress.write(f"{key}: computing footprint...")
            entry["footprint"] = compute_footprint(trace)
            save_results(data, output)

        footprint = int(entry["footprint"] or 0)
        if footprint <= 0:
            progress.write(f"{key}: WARNING footprint=0; skipping")
            continue

        large_size = max(1, int(large_frac * footprint))
        small_size = max(1, int(small_frac * footprint))
        entry["large_size"] = large_size
        entry["small_size"] = small_size

        miss_ratios: dict[str, dict[str, float]] = entry.setdefault("miss_ratios", {})

        # Identify missing simulations
        missing: dict[str, list[int]] = {}
        for algo in all_algos:
            algo_cache = miss_ratios.get(algo, {})
            target_sizes = list(dict.fromkeys((large_size, small_size)))
            needed = [
                sz
                for sz in target_sizes
                if str(sz) not in algo_cache
            ]
            if needed:
                missing[algo] = needed

        if missing:
            progress.set_postfix_str(f"{key} | simulating")
            progress.write(f"{key}: simulating {sorted(missing.keys())} ...")
            results = simulate_trace(trace, missing)
            for algo, size_mrs in results.items():
                algo_cache = miss_ratios.setdefault(algo, {})
                algo_cache.update({str(sz): float(mr) for sz, mr in size_mrs.items()})
            save_results(data, output)
        else:
            progress.set_postfix_str(f"{key} | cached")
            progress.write(f"{key}: using cached results")

    return data


def main() -> None:
    p = argparse.ArgumentParser(
        description="Run cache algorithm simulations and output a JSON results file"
    )
    p.add_argument(
        "--trace-dir",
        required=True,
        help="Directory containing local oracleGeneral traces (.bin/.zst)",
    )
    p.add_argument(
        "--algos",
        default="lru",
        help=f"Comma-separated algos to run (FIFO baseline is always included). Supported: {sorted(CACHE_ALGORITHMS)}",
    )
    p.add_argument("--large-frac", type=float, default=0.10, help="Large cache size as fraction of footprint (default 0.10)")
    p.add_argument("--small-frac", type=float, default=0.001, help="Small cache size as fraction of footprint (default 0.001)")
    p.add_argument("--output", default="results/hit_rate.json", help="Output JSON path")
    p.add_argument(
        "--trace-globs",
        default="*.bin,*.zst",
        help=(
            "Comma-separated glob patterns to select trace files within --trace-dir "
            "(default: '*.bin,*.zst'). Use '*.bin' if you don't have the zstandard "
            "dependency installed."
        ),
    )
    p.add_argument(
        "--no-resume",
        dest="resume",
        action="store_false",
        default=True,
        help="Do not reuse/append to an existing output JSON (default: resume)",
    )
    args = p.parse_args()

    algos = [a.strip().lower() for a in args.algos.split(",") if a.strip()]
    unknown = [a for a in algos if a not in CACHE_ALGORITHMS]
    if unknown:
        sys.exit(f"Unknown algo(s): {unknown}. Supported: {sorted(CACHE_ALGORITHMS)}")

    output = Path(args.output)
    trace_globs = [g.strip() for g in str(args.trace_globs).split(",") if g.strip()]

    trace_dir = Path(args.trace_dir)
    if not trace_dir.is_dir():
        sys.exit(f"Trace directory not found: {trace_dir}")
    trace_sources = _collect_local_trace_sources(trace_dir, trace_globs)
    trace_source_label = f"dir:{trace_dir}"

    if not trace_sources:
        sys.exit(f"No trace files found in {trace_dir} matching {trace_globs}")

    data = run(
        trace_sources=trace_sources,
        algos=algos,
        large_frac=float(args.large_frac),
        small_frac=float(args.small_frac),
        output=output,
        resume=bool(args.resume),
        trace_source_label=trace_source_label,
    )

    save_results(data, output)
    print(f"\nWrote results JSON to {output}")


if __name__ == "__main__":
    main()