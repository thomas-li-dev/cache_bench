#!/bin/bash
# Run single-threaded BitSIEVE vs SIEVE capacity sweep benchmark.
# Usage: ./scripts/run_capacity_sweep.sh [traces...]
#   traces: meta22, meta, twitter (default: meta22)
#
# This script:
# 1. Patches main.cpp to sweep capacity proportions
# 2. Builds and runs the benchmark
# 3. Moves results to a timestamped file
# 4. Restores main.cpp

set -euo pipefail
cd "$(dirname "$0")/.."

TRACES="${@:-meta22}"
RESULTS="results_capacity_sweep.json"

# Backup main.cpp
cp main.cpp main.cpp.bak

# Generate a main.cpp for the capacity sweep
cat > main.cpp << 'MAIN_EOF'
#include "bench.h"
#include "sieve_bit.h"
#include "sieve_single.h"

const std::string twi_trace_location = "twi_proc/";
const std::string meta_trace_location = "meta_proc/";
const std::string meta22_trace_location = "meta22_proc/";

int main() {
  std::vector<size_t> threads_choices = {1};
  std::vector<double> prop{0.01, 0.025, 0.05, 0.075, 0.1, 0.15, 0.2, 0.3, 0.5};
  std::vector<scale_policy> policies{scale_policy::TRANSFORM_SPACE};

  Bench b(threads_choices, prop, policies);
  b.add_cache<SIEVESingle>("SIEVE");
  b.add_cache<SIEVEBit<8>>("BitSIEVE");

MAIN_EOF

# Add requested traces
for trace in $TRACES; do
  case "$trace" in
    meta22)  echo "  b.add_trace(\"meta22\", meta22_trace_location, 3);" >> main.cpp ;;
    meta)    echo "  b.add_trace(\"meta\", meta_trace_location, 3);" >> main.cpp ;;
    twitter) echo "  b.add_trace(\"twitter\", twi_trace_location, 3);" >> main.cpp ;;
    *)       echo "Unknown trace: $trace" >&2; exit 1 ;;
  esac
done

cat >> main.cpp << 'MAIN_EOF'
  b.run();
}
MAIN_EOF

echo "=== Building benchmark ==="
make -j$(nproc)

echo "=== Running capacity sweep ==="
./cache_bench

# Move results
mv results.json "$RESULTS"
echo "=== Results saved to $RESULTS ==="

# Restore main.cpp
mv main.cpp.bak main.cpp
echo "=== main.cpp restored ==="

echo ""
echo "To generate plots, run:"
echo "  python3 scripts/plot_sieve_comparison.py --input $RESULTS"
