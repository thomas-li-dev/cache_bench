#pragma once
#include "types.h"
#include <functional>
class ICache {
public:
  // TODO: is there a way to enforce a specific constructor signature?
  // We should always have capacity as first argument.
  virtual ~ICache() = default;
  // TODO: having std::function overhead here is bad.
  // this interface will likely change a lot.
  virtual cache_token_t
  query(cache_key_t k, std::function<cache_token_t(cache_key_t)> get_token) = 0;

  // so ugly.
  virtual size_t get_cap() const = 0;
};
