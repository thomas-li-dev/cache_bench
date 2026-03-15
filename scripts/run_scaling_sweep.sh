#!/bin/bash
# Run multi-threaded scaling benchmark: FIFO vs SIEVE vs LRU on twitter trace.
# Usage: ./scripts/run_scaling_sweep.sh
#
# Tests thread counts 1,2,4,8,16 at 10% capacity.

set -euo pipefail
cd "$(dirname "$0")/.."

RESULTS="results_scaling_sweep.json"

# Backup main.cpp
cp main.cpp main.cpp.bak

cat > main.cpp << 'MAIN_EOF'
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

  b.add_trace("twitter", twi_trace_location);
  b.run();
}
MAIN_EOF

echo "=== Building benchmark ==="
make -j$(nproc)

echo "=== Running scaling sweep ==="
./cache_bench

mv results.json "$RESULTS"
echo "=== Results saved to $RESULTS ==="

mv main.cpp.bak main.cpp
echo "=== main.cpp restored ==="

echo ""
echo "To generate plots, run:"
echo "  python3 scripts/plot_scaling.py --input $RESULTS"
