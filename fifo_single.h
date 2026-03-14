#pragma once
#include "types.h"
#include <boost/unordered/unordered_flat_map.hpp>
#include <cassert>
#include <vector>
using namespace boost::unordered;

class FIFOSingle {
private:
  size_t cap;
  size_t siz{};
  size_t head{}; // next insertion slot (also oldest entry when full)
  std::vector<cache_key_t> keys;
  std::vector<cache_token_t> tokens;
  unordered_flat_map<cache_key_t, size_t> map;

public:
  explicit FIFOSingle(size_t cap) : cap(cap), keys(cap), tokens(cap) {
    assert(cap > 0);
    map.reserve(cap + 2);
  }

  cache_token_t query(cache_key_t k, cache_token_t (*get_token)(void *),
                      void *ctx) {
    auto itr = map.find(k);
    if (itr != map.end())
      return tokens[itr->second];

    cache_token_t t = get_token(ctx);

    if (siz == cap) {
      map.erase(keys[head]);
    } else {
      siz++;
    }

    keys[head] = k;
    tokens[head] = t;
    map[k] = head;

    if (++head == cap)
      head = 0;

    return t;
  }

  size_t get_cap() const { return cap; }
  static constexpr bool can_multithread() { return false; }
};
