from __future__ import annotations

from collections import OrderedDict

from . import _register
from .base import CacheAlgo

class SIEVENode:
    def __init__(self, value):
        self.value = value
        self.visited = False
        self.next = None
        self.prev = None

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

    def get(self, obj_id: int) -> bool:
        # Item in cache, set visited flag
        if obj_id in self.cache: 
            self.cache[obj_id].visited = True
            return True

        # if our cache is full, evict an element to make space 
        if self.size == self.cache_size:
            self._evict()

        # Eviction
        node = SIEVENode(obj_id)
        node.next = self.head
        node.prev = None
        if self.head:
            self.head.prev = node
        self.head = node
        if self.tail is None:
            self.tail = node

    def _evict(self):
        to_remove = self.hand if self.hand else self.tail
        while(to_remove != None and to_remove.visited == True):
            # Unset visited
            obj.visited = False
            obj = obj.prev if obj.prev else self.tail
        self.hand = obj.prev if obj.prev else None
        
        # Remove the object
        del self.cache[obj.value]
        if obj.prev:
            obj.prev.next = obj.next
        else:
            self.head = obj.next
        if obj.next:
            obj.next.prev = obj.prev
        else:
            self.tail = obj.prev
        
        self.size -= 1