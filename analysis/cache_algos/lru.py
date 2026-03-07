from __future__ import annotations

from collections import OrderedDict

from . import _register
from .base import CacheAlgo

@_register("lru")
class LRU(CacheAlgo):

    def __init__(self, cache_size: int):
        super().__init__(cache_size)
        # obj_id -> True, order encodes LRU(front) -> MRU(back)
        self._contents: OrderedDict[int, bool] = OrderedDict()

    def get(self, obj_id: int) -> bool:
        self._num_requests += 1

        if self.cache_size <= 0:
            self._num_misses += 1
            return False

        if obj_id in self._contents:
            self._contents.move_to_end(obj_id)  # promote to MRU
            return True

        self._num_misses += 1

        if len(self._contents) >= self.cache_size:
            self._contents.popitem(last=False)  # evict LRU

        self._contents[obj_id] = True
        return False