from __future__ import annotations

from collections import OrderedDict

from . import _register
from .base import CacheAlgo
from threading import RLock

class SIEVENode:
    def __init__(self, value):
        self.value = value
        self.visited = False
        self.next = None
        self.prev = None

        self.lock = RLock()

@_register("sieve")
class SIEVE(CacheAlgo):

    def __init__(self, cache_size: int):
        super().__init__(cache_size)

        # Store hashmap and doubly linked list of data
        self.cache = {}
        self.head = None
        self.tail = None
        
        self.hand = None
        self.size = 0

        self.rlock = threading.RLock()

    def get(self, obj_id: int) -> bool:
        with self.rlock:
            # Item in cache, set visited flag
            if obj_id in self.cache: 
                self.cache[obj_id].visited = True
                return True

            if self.cache_size == 0:
                self._num_misses += 1
                return False

            # if our cache is full, evict an element to make space 
            if self.size == self.cache_size:
                self._evict()

            node = SIEVENode(obj_id)
            node.next = self.head
            node.prev = None
            if self.head:
                self.head.prev = node
            self.head = node
            if self.tail is None:
                self.tail = node

            self.cache[obj_id] = node
            self.size += 1
            node.visited = False 

            return False

    def _evict(self):
        with self.rlock:
            to_remove = self.hand if self.hand else self.tail
            while(to_remove != None and to_remove.visited == True):
                # Unset visited
                to_remove.visited = False
                to_remove = to_remove.prev if to_remove.prev else self.tail
            if(to_remove == None):
                # We should never reach this but just in case
                return
            self.hand = to_remove.prev if to_remove.prev else None
            
            # Remove the evicted element from our cache
            del self.cache[to_remove.value]
            if to_remove.prev:
                to_remove.prev.next = to_remove.next
            else:
                self.head = to_remove.next
            if to_remove.next:
                to_remove.next.prev = to_remove.prev
            else:
                self.tail = to_remove.prev
            
            self.size -= 1