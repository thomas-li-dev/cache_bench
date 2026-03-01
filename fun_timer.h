#pragma once
#include "metrics.h"
#include <chrono>
#include <map>
#include <string>

class FunTimer {
private:
  std::map<std::string, Metrics> m;

public:
  template <class F> auto run(const std::string &name, F &&f) {
    auto start = std::chrono::high_resolution_clock::now();
    auto res = f();
    auto end = std::chrono::high_resolution_clock::now();
    m[name].add(std::chrono::duration<double, std::micro>(end - start).count());
    return res;
  }
  void print() {
    for (auto &[name, metrics] : m) {
      std::println("for fun {}", name);
      metrics.print();
    }
  }
};
