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
  std::list<key> l;
  std::unordered_map<key, std::pair<token, std::list<key>::iterator>> m;

  void evict() {
    assert(l.size());
    auto k = l.back();
    l.pop_back();
    m.erase(k);
  }
  bool in(key key) { return m.contains(key); }
  void add(key key, token t) {
    assert(l.size() < c);
    auto itr = l.insert(l.begin(), key);
    m[key] = {t, itr};
  }
  bool can_add() const { return m.size() < c; }

public:
  LRU(size_t cap) : c(cap) {}

  token query(key k, std::function<token(key)> get_token) override {
    if (in(k)) {
      auto [t, itr] = m[k];
      l.splice(l.begin(), l, itr);
      return t;
    }
    token t = get_token(k);
    if (!can_add())
      evict();
    add(k, t);
    return t;
  }
};