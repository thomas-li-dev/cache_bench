#pragma once
#include "cache.h"
#include <functional>
#include <unordered_map>

class BadCache : public ICache {
public:
  cache_token_t
  query(cache_key_t k,
        std::function<cache_token_t(cache_key_t)> get_token) override {
    return get_token(k);
  }
};