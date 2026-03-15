from __future__ import annotations

import heapq
from typing import Iterable
from trace_parser import read_requests_next_access

INF = (1 << 63) - 1


def belady_single_pass_from_reader(tracepath: str, capacity: int) -> tuple[int, int]:
    """
    Exact 1-pass Belady, assuming each request already includes next_access_vtime.

    Expected record format from read_requests(tracepath):
        (timestamp, obj_id, obj_size, next_access_vtime)

    where:
        - obj_id is uint64-compatible
        - next_access_vtime is int64
        - next_access_vtime == -1 means "no future access"

    Returns:
        (faults, num_requests)
    """
    if capacity < 0:
        raise ValueError("capacity must be >= 0")

    faults = 0
    n = 0

    cache: set[int] = set()
    current_next: dict[int, int] = {}

    # max-heap by next-use, implemented as min-heap of negative next-use
    # entries: (-next_use, obj_id)
    heap: list[tuple[int, int]] = []

    for _, obj_id, _, next_access_vtime in read_requests(tracepath):
        n += 1
        obj_id = int(obj_id)

        nxt = INF if int(next_access_vtime) == -1 else int(next_access_vtime)

        if capacity == 0:
            faults += 1
            continue

        if obj_id in cache:
            current_next[obj_id] = nxt
            heapq.heappush(heap, (-nxt, obj_id))
            continue

        faults += 1

        if len(cache) >= capacity:
            while True:
                neg_next, victim = heapq.heappop(heap)
                victim_next = -neg_next

                # lazy deletion of stale heap entries
                if victim in cache and current_next.get(victim) == victim_next:
                    break

            cache.remove(victim)
            del current_next[victim]

        cache.add(obj_id)
        current_next[obj_id] = nxt
        heapq.heappush(heap, (-nxt, obj_id))

    return faults, n


if __name__ == "__main__":
    tracepath = "analysis/traces/202206_kv_traces_all.csv.oracleGeneral.csv"
    faults, n = belady_single_pass_from_reader(tracepath, capacity=100)
    print(f"faults={faults}, requests={n}, miss_rate={faults / n:.6f}")