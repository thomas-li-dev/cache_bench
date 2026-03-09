from __future__ import annotations

from collections import OrderedDict

from . import _register
from .base import CacheAlgo

@_register("arc")
class ARC(CacheAlgo):

    def __init__(self, cache_size: int):
        super().__init__(cache_size)

        self._B1: OrderedDict[int, bool] = OrderedDict()
        self._B2: OrderedDict[int, bool] = OrderedDict()

        self._T1: OrderedDict[int, bool] = OrderedDict()
        self._T2: OrderedDict[int, bool] = OrderedDict()

        self._p: int = 0

    def replace(self, obj_id: int):
        if len(self._T1) > 0 and (len(self._T1) > self._p or (obj_id in self._B2 and len(self._T1) == self._p)):
            victim, _ = self._T1.popitem(last=False)
            self._B1[victim] = True
        else:
            victim, _ = self._T2.popitem(last=False)
            self._B2[victim] = True

    def get(self, obj_id: int) -> bool:
        self._num_requests += 1

        if obj_id in self._T1 or obj_id in self._T2:
            if obj_id in self._T1:
                self._T1.pop(obj_id)
            elif obj_id in self._T2:
                self._T2.pop(obj_id)

            self._T2[obj_id] = True
            return True
        elif obj_id in self._B1:
            self._num_misses += 1
            delta = 1 if len(self._B1) >= len(self._B2) else len(self._B2) // len(self._B1)
            self._p = min(self._p + delta, self.cache_size)
            self.replace(obj_id)
            self._B1.pop(obj_id)
            self._T2[obj_id] = True
            return False
        elif obj_id in self._B2:
            self._num_misses += 1
            delta = 1 if len(self._B2) >= len(self._B1) else len(self._B1) // len(self._B2)
            self._p = max(self._p - delta, 0)
            self.replace(obj_id)
            self._B2.pop(obj_id)
            self._T2[obj_id] = True
            return False
        else:
            self._num_misses += 1
            if len(self._B1) + len(self._T1) == self.cache_size:
                if len(self._T1) < self.cache_size:
                    self._B1.popitem(last=False)
                    self.replace(obj_id)
                else:
                    self._T1.popitem(last=False)
            elif len(self._B1) + len(self._T1) < self.cache_size:
                if len(self._T1) + len(self._T2) + len(self._B1) + len(self._B2) >= self.cache_size:
                    if len(self._T1) + len(self._T2) + len(self._B1) + len(self._B2) == 2 * self.cache_size:
                        self._B2.popitem(last=False)
                    self.replace(obj_id)
            self._T1[obj_id] = True
            return False