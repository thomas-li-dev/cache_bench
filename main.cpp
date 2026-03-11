#include "bad_cache.h"
#include "bench.h"
#include "fifo.h"
#include "lru.h"

const std::string twi_trace_location = "twi_proc/";
int main() {
  std::vector<size_t> threads_choices = {1, 2, 4, 8};
  std::vector<double> prop{0.1, 0.01, 0.001};
  Bench b(threads_choices, prop);
  b.add_cache<FIFO>("FIFO");
  b.add_cache<LRU>("LRU");
  // b.add_cache<BadCache>("BadCache");
  b.add_trace("twitter", twi_trace_location, 1);
  b.run();
}
