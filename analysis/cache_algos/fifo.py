from __future__ import annotations

from collections import deque

from . import _register
from .base import CacheAlgo

@_register("fifo")
class FIFO(CacheAlgo):

    def __init__(self, cache_size: int):
        super().__init__(cache_size)
        self._occupied = 0
        self._order: deque[int] = deque()
        self._contents: set[int] = set()

    def get(self, obj_id: int) -> bool:
        self._num_requests += 1

        if self.cache_size <= 0:
            self._num_misses += 1
            return False

        if obj_id in self._contents:
            return True

        self._num_misses += 1

        if self._order and self._occupied >= self.cache_size:
            evicted_id = self._order.popleft()
            self._contents.discard(evicted_id)
            self._occupied -= 1

        self._order.append(obj_id)
        self._contents.add(obj_id)
        self._occupied += 1
        return False

    