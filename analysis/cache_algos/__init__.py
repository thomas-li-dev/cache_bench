from __future__ import annotations

from typing import Callable

from .base import CacheAlgo

CACHE_ALGORITHMS: dict[str, type[CacheAlgo]] = {}

def _register(name: str) -> Callable[[type[CacheAlgo]], type[CacheAlgo]]:

    def decorator(cls: type[CacheAlgo]) -> type[CacheAlgo]:
        CACHE_ALGORITHMS[name.lower()] = cls
        return cls

    return decorator

from .fifo import FIFO
from .lru import LRU
from .clock import Clock
from .twoq import TwoQ
from .sieve import Sieve
from .arc import ARC
from .ghostsieve import GhostSieve