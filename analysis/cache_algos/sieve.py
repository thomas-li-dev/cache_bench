from __future__ import annotations

from collections import OrderedDict

from . import _register
from .base import CacheAlgo

@_register("sieve")
class Sieve(CacheAlgo):

    def __init__(self, cache_size: int):
        super().__init__(cache_size)
        self._contents: OrderedDict[int, bool] = OrderedDict()

    def get(self, obj_id: int) -> bool:
        self._num_requests += 1

        if obj_id in self._contents:
            self._contents[obj_id] = True
            return True

        self._num_misses += 1

        while len(self._contents) >= self.cache_size:
            victim_id, victim_visited = self._contents.popitem(last=False)
            if victim_visited:
                self._contents[victim_id] = False
                continue
        
        self._contents[obj_id] = False
        return False