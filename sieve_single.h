#pragma once
#include "cache.h"
#include "types.h"
#include <atomic>
#include <boost/unordered/concurrent_flat_map.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <cassert>
#include <list>
#include <mutex>
#include <cstdio>
using namespace boost::unordered;
class SIEVESingle : public ICache {
private:
  struct ListData {
    bool vis;
    cache_key_t k;
    cache_token_t t;
    ListData *nxt, *prv;
  };
  ListData *head, *hand;
  int64_t cap;
  int64_t siz{};
  unordered_flat_map<cache_key_t, ListData *> map;

public:
  explicit SIEVESingle(size_t cap) : cap(cap) {
    assert(cap > 0);
    head = new ListData{0, 0, 0, 0, 0};
    head->nxt = head->prv = head;
    hand = head;
  }

  cache_token_t query(cache_key_t k, cache_token_t (*get_token)(void *),
                      void *ctx) override {
    cache_token_t t;
    auto itr = map.find(k);
    if (itr != map.end()) {
      t = itr->second->t;
      itr->second->vis = true;
      return t;
    }
    t = get_token(ctx);
    ListData *ld = new ListData{0, k, t, 0, 0};
    map[k] = ld;
    siz++;
    auto nxt = head->nxt;
    head->nxt = ld;
    nxt->prv = ld;
    ld->nxt = nxt;
    ld->prv = head;

    size_t steps = 0;
    while (siz > cap) {
      steps++;
      // evict smth
      // to end
      if (hand == head)
        hand = head->prv;
      if (hand->vis) {
        hand->vis = false;
        hand = hand->prv;
        continue;
      }
      cache_key_t to_evict = hand->k;
      auto prv = hand->prv, nxt = hand->nxt;
      prv->nxt = nxt;
      nxt->prv = prv;
      siz--;
      size_t sbo = map.erase(to_evict);
      assert(sbo == 1);
      delete hand;
      hand = prv;
    }
    static int iter = 0;
    if (false && ++iter % 1024 == 0)
      printf("steps %zu\n\n", steps);
    return t;
  }

  size_t get_cap() const override { return cap; }
  static bool can_multithread() { return false; }
};