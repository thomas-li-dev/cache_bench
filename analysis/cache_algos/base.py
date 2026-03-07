from __future__ import annotations

from abc import ABC, abstractmethod

class CacheAlgo(ABC):

    def __init__(self, cache_size: int):
        self.cache_size = cache_size
        self._num_requests = 0
        self._num_misses = 0

    @abstractmethod
    def get(self, obj_id: int) -> bool:
        """Process one request. Returns True on hit, False on miss."""

    @property
    def miss_ratio(self) -> float:
        return (self._num_misses / self._num_requests) if self._num_requests else 0.0