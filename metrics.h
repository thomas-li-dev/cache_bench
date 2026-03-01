#pragma once
#include <limits>
#include <print>

// TODO: mn and avg is useless due to influence of queries before steady state.
// Can we make it take the last K samples?
// Also add approximate quartiles
struct Metrics {
  double sum = 0, cnt = 0, mn = std::numeric_limits<double>::max(),
         mx = std::numeric_limits<double>::lowest();
  void add(double t) {
    sum += t;
    cnt++;
    mn = std::min(mn, t);
    mx = std::max(mx, t);
  }
  void print() {
    if (cnt == 0) {
      std::println("no metrics");
      return;
    }
    std::println("avg = {}, mn = {}, mx = {}", sum / cnt, mn, mx);
  }
};
