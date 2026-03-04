#pragma once
#include "cache.h"
#include <functional>
#include <unordered_map>

class BadCache : public ICache {
public:
  token query(key k, std::function<token(key)> get_token) override {
    return get_token(k);
  }
};