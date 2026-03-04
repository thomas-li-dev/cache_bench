#pragma once
#include "types.h"
#include <functional>
#include <string>
#include <string_view>
class ITrace {
public:
  virtual ~ITrace() = default;
  virtual void run_each_query(std::function<void(key_type)> to_run) = 0;
};
