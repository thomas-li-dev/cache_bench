#pragma once
#include "cache.h"
#include <cassert>
#include <functional>
#include <list>
#include <queue>
#include <unordered_map>

class LRU : public ICache {
private:
  size_t c;
  std::list<cache_key_t> l;
  std::unordered_map<cache_key_t,
                     std::pair<cache_token_t, std::list<cache_key_t>::iterator>>
      m;

  void evict() {
    assert(l.size());
    auto k = l.back();
    l.pop_back();
    m.erase(k);
  }
  bool in(cache_key_t key) { return m.contains(key); }
  void add(cache_key_t key, cache_token_t t) {
    assert(l.size() < c);
    auto itr = l.insert(l.begin(), key);
    m[key] = {t, itr};
  }
  bool can_add() const { return m.size() < c; }

public:
  LRU(size_t cap) : c(cap) {}

  cache_token_t
  query(cache_key_t k,
        std::function<cache_token_t(cache_key_t)> get_token) override {
    if (in(k)) {
      auto [t, itr] = m[k];
      l.splice(l.begin(), l, itr);
      return t;
    }
    cache_token_t t = get_token(k);
    if (!can_add())
      evict();
    add(k, t);
    return t;
  }
};