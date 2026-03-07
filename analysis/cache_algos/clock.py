from __future__ import annotations

from collections import deque

from . import _register
from .base import CacheAlgo

@_register("clock")
class Clock(CacheAlgo):

    def __init__(self, cache_size: int):
        super().__init__(cache_size)
        # Queue order approximates a circular list; front is current hand.
        self._queue: deque[int] = deque()
        self._ref: dict[int, bool] = {}

    def get(self, obj_id: int) -> bool:
        self._num_requests += 1

        if self.cache_size <= 0:
            self._num_misses += 1
            return False

        if obj_id in self._ref:
            self._ref[obj_id] = True
            return True

        self._num_misses += 1

        # Second-chance scan: rotate objects with ref bit set, evict first with ref=0.
        while len(self._queue) >= self.cache_size:
            victim = self._queue[0]
            if self._ref.get(victim, False):
                self._ref[victim] = False
                self._queue.rotate(-1)
                continue
            self._queue.popleft()
            self._ref.pop(victim, None)

        self._queue.append(obj_id)
        self._ref[obj_id] = True
        return False