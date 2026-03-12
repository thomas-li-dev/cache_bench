#pragma once
#include "cache.h"
#include <functional>
#include <unordered_map>

class BadCache : public ICache {
private:
  size_t cap_;

public:
  BadCache(size_t cap) : cap_(cap) {}
  cache_token_t query(cache_key_t k [[maybe_unused]],
                      std::function<cache_token_t()> get_token) override {
    return get_token();
  }
  virtual size_t get_cap() const override { return cap_; }
  static bool can_multithread() { return true; }
};