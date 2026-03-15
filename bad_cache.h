#pragma once
#include "types.h"
#include <cstddef>

class BadCache {
private:
  size_t cap_;

public:
  BadCache(size_t cap) : cap_(cap) {}
  cache_token_t query(cache_key_t k [[maybe_unused]],
                      cache_token_t (*get_token)(void *), void *ctx) {
    return get_token(ctx);
  }
  size_t get_cap() const { return cap_; }
  static constexpr bool can_multithread() { return true; }
};