// give set of eviction algos, and set of traces
// for each trace, run all eviction algos
// collect metrics
#include "cache_runner.h"
#include "fifo.h"
#include "trace.h"
#include <memory>
#include <vector>

// TODO: add multithreading
// one method: generate a trace for each thread
// all threads share a cache.
// like if threads have different caches it seems equivalent to just running a
// single thread...

class Bench {
private:
  std::vector<CacheRunner> caches;
  std::vector<std::pair<std::string, std::unique_ptr<ITrace>>> traces;

public:
  void run() {
    for (auto &[name, trace] : traces) {
      std::println("running trace {}", name);
      int qp = 0;
      trace->run_each_query([&](key_type key) {
        for (auto &cache : caches) {
          cache.do_query(key);
        }
        if (++qp % 1000000 == 0) {
          std::println("processed {} queries", qp);
          /*
          for (auto &cache : caches)
            cache.print();*/
          // TODO: have proper way to warm up and then measure.
          /*
        for (auto &cache : caches)
          cache.reset();*/
        }
      });
    }
    for (auto &cache : caches)
      cache.print();
  }
  template <class T, class... Args>
  void add_cache(const std::string &name, Args &&...args) {
    caches.push_back(CacheRunner(name, std::make_unique<T>(args...)));
  }
  template <class T, class... Args>
  void add_trace(const std::string &name, Args &&...args) {
    traces.push_back({name, std::make_unique<T>(args...)});
  }
};