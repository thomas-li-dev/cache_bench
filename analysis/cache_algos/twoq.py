from __future__ import annotations

from collections import OrderedDict

from . import _register
from .base import CacheAlgo

@_register("twoq")
class TwoQ(CacheAlgo):

    def __init__(self, cache_size: int):
        super().__init__(cache_size)
        self._Am: OrderedDict[int, bool] = OrderedDict()
        self._A1_in: OrderedDict[int, bool] = OrderedDict()
        self._A1_out: OrderedDict[int, bool] = OrderedDict()
        # Am is roughly 80% of the cache size
        self._Am_cache_size: int = cache_size // 10 * 8
        # A1_in is roughly 20% of the cache size
        self._A1_in_cache_size: int = cache_size - self._Am_cache_size
        # A1_out is the entire cache size (since it only contains references)
        self._A1_out_cache_size: int = cache_size

    def get(self, obj_id: int) -> bool:
        self._num_requests += 1

        if self.cache_size <= 0:
            self._num_misses += 1
            return False

        if obj_id in self._Am:
            self._Am.move_to_end(obj_id)  # promote to MRU
            return True
        elif obj_id in self._A1_in:
            if len(self._Am) >= self._Am_cache_size:
                _ = self._Am.popitem(last=False)  # evict LRU in Am

            self._A1_in.pop(obj_id)
            self._Am[obj_id] = True
            return True
        elif obj_id in self._A1_out:
            if len(self._Am) >= self._Am_cache_size:
                _ = self._Am.popitem(last=False)  # evict LRU in Am

            self._A1_out.pop(obj_id)
            self._Am[obj_id] = True
            self._num_misses += 1
            return False

        if len(self._A1_in) >= self._A1_in_cache_size:
            evicted_id, _ = self._A1_in.popitem(last=False)  # evict LRU in A1_in
            if len(self._A1_out) >= self._A1_out_cache_size:
                _ = self._A1_out.popitem(last=False)  # evict LRU in A1_out
            self._A1_out[evicted_id] = True # promote to A1_out

        self._A1_in[obj_id] = True
        self._num_misses += 1
        return False