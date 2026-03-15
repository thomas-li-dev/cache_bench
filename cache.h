#pragma once
#include "types.h"
#include <cstddef>
class ICache {
public:
  // TODO: is there a way to enforce a specific constructor signature?
  // We should always have capacity as first argument.
  virtual ~ICache() = default;
  virtual cache_token_t query(cache_key_t k,
                              cache_token_t (*get_token)(void *),
                              void *ctx) = 0;

  // so ugly.
  virtual size_t get_cap() const = 0;
};
