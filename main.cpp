#include "bench.h"
#include "fifo.h"
#include "lru.h"
#include "sieve.h"

const std::string twi_trace_location = "twi_proc/";

int main() {
  std::vector<size_t> threads_choices = {1, 2, 4, 8, 16};
  std::vector<double> prop{0.1};
  std::vector<scale_policy> policies{
      scale_policy::TRANSFORM_SPACE,
  };

  Bench b(threads_choices, prop, policies);
  b.add_cache<FIFO>("FIFO");
  b.add_cache<SIEVE>("SIEVE");
  b.add_cache<LRU>("LRU");

  b.add_trace("twitter", twi_trace_location, 3);
  b.run();
}
