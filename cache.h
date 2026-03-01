#pragma once
#include "types.h"

class ICache {
public:
  virtual ~ICache() = default;
  virtual void evict() = 0;
  virtual bool in(key_type key) = 0;
  virtual void add(key_type key) = 0;
  virtual bool can_add() const = 0;
};
