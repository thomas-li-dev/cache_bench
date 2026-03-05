#pragma once
#include "types.h"
#include <functional>
class ICache {
public:
  virtual ~ICache() = default;
  virtual cache_token_t
  query(cache_key_t k, std::function<cache_token_t(cache_key_t)> get_token) = 0;
};
