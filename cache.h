#pragma once
#include "types.h"
#include <functional>
class ICache {
public:
  virtual ~ICache() = default;
  virtual token query(key k, std::function<token(key)> get_token) = 0;
};
