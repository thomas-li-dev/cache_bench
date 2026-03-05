#include "bad_cache.h"
#include "bench.h"
#include "fifo.h"
#include "lru.h"
#include "twi_trace.h"
#include <fcntl.h>

const std::string twi_trace_location = "twi_traces/";
int main() {
  std::vector<size_t> threads_choices = {1, 2, 4, 8};
  std::vector<size_t> cap_choices = {1 << 9, 1 << 10, 1 << 11, 1 << 12};
  Bench b(threads_choices, cap_choices);
  b.add_cache<FIFO>("FIFO");
  b.add_cache<LRU>("LRU");
  b.add_cache<BadCache>("BadCache");
  b.add_trace<TwiTrace>("twitter", twi_trace_location);
  b.run();
}
