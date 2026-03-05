#pragma once
#include "types.h"
#include <optional>
#include <vector>
class ITrace {
public:
  virtual ~ITrace() = default;
  virtual std::optional<cache_key_t> next_key() = 0;
  virtual void reset() = 0;
  // similar interface to read syscall
  size_t to_buf(std::vector<cache_key_t> &buf) {
    for (size_t i = 0; i < buf.size(); i++) {
      auto k = next_key();
      if (k) {
        buf[i] = *k;
      } else
        return i;
    }
    return buf.size();
  }
};
