#pragma once
#include "types.h"
#include <optional>
#include <vector>
class ITrace {
public:
  virtual ~ITrace() = default;
  virtual std::optional<cache_key_t> next_key() = 0;
  virtual size_t next_buf(std::vector<cache_key_t> &buf) = 0;
  virtual void reset() = 0;
};
