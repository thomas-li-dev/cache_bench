#pragma once
#include "cache.h"
#include <cstddef>

class BadCache : public ICache {
private:
  size_t cap_;

public:
  BadCache(size_t cap) : cap_(cap) {}
  cache_token_t query(cache_key_t k [[maybe_unused]],
                      cache_token_t (*get_token)(void *), void *ctx) override {
    return get_token(ctx);
  }
  virtual size_t get_cap() const override { return cap_; }
  static bool can_multithread() { return true; }
};