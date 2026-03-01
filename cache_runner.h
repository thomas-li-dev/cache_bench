#pragma once
#include "cache.h"
#include "fun_timer.h"

class CacheRunner {
private:
  std::string n;
  ICache *c;
  FunTimer ft;
  int queries = 0, hits = 0;

public:
  CacheRunner(std::string name, ICache *cache) : n(name), c(cache) {}
  // TODO: runtime polymorphism has overhead.
  void do_query(key_type key) {
    queries++;
    auto in_l = [&]() { return c->in(key); };
    auto can_add_l = [&]() { return c->can_add(); };
    auto evict_l = [&]() {
      c->evict();
      return 0;
    };
    auto add_l = [&]() {
      c->add(key);
      return 0;
    };
    if (ft.run("in", in_l)) {
      hits++;
      return;
    }
    if (!ft.run("can_add", can_add_l))
      ft.run("evict", evict_l);
    ft.run("add", add_l);
  }
  void print() {
    std::println("======== {} =======", n);
    if (queries == 0) {
      std::println("no queries");
      return;
    }
    ft.print();
    std::println("hitrate = {}", 1.0 * hits / queries);
  }
};
