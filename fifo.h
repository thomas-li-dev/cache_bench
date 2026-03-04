#pragma once
#include "cache.h"
#include <cassert>
#include <queue>
#include <unordered_set>

class FIFO : public ICache {
private:
  size_t c;
  std::queue<key_type> q;
  std::unordered_set<key_type> s;

public:
  FIFO(size_t cap) : c(cap) {}

  void evict() override {
    assert(q.size());
    auto k = q.front();
    q.pop();
    s.erase(k);
  }
  bool in(key_type key) override { return s.contains(key); }
  void add(key_type key) override {
    assert(q.size() < c);
    s.insert(key);
    q.push(key);
  }
  bool can_add() const override { return s.size() < c; }
};
