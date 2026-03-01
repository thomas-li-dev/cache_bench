#include "cache_runner.h"
#include "fifo.h"
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <print>
#include <sys/mman.h>
#include <unistd.h>

const std::string twi_trace_location = "twi_traces/";
/*
Trace Format

The original traces are plain text structured as comma-separated columns. Each
row represents one request in the following format.

    timestamp: the time when the cache receives the request, in sec
    anonymized key: the original key with anonymization
    key size: the size of key in bytes
    value size: the size of value in bytes, could be 0 if it is a cache miss
    client id: the anonymized clients (frontend service) who sends the request
    operation: one of
get/gets/set/add/replace/cas/append/prepend/delete/incr/decr
    TTL: the
time-to-live (TTL) of the object set by the client, it is 0 when the request is
not a write request.

only key is important.
check operation to only take gets.
*/

std::vector<CacheRunner> to_test;

// TODO: add multithreading
// one method: generate a trace for each thread
// all threads share a cache.
// like if threads have different caches it seems equivalent to just running a
// single thread...
void run_twi_trace(std::string_view s) {
  size_t qp = 0;
  auto run_query = [&](key_type k) {
    qp++;
    for (auto &cr : to_test)
      cr.do_query(k);
    if (qp % 10000000 == 0) {
      std::println("processed {} queries", qp);
      for (auto &cr : to_test) {
        cr.print();
      }
    }
  };
  // process each query.
  // avoid allocating any more strings
  // probably could be parsed faster with simd :monkey:
  for (size_t i = 0; i < s.size(); i++) {
    auto j = i;
    while (j + 1 < s.size() && s[j + 1] != '\n')
      j++;

    // [i,j] is a line.
    int which = 0;

    uint64_t h = 0;
    bool skip = 0;

    for (size_t k = i, st = i; k <= j + 1; k++) {
      if (k > j || s[k] == ',') {
        if (which == 1) {
          // compute a simple rolling hash of the string.
          // trivial to hack but should be fine for the trace.
          h = 0;
          for (size_t i = st; i < k; i++) {
            h = h * 31 + s[i];
          }
        } else if (which == 5) {
          // skip none get / gets operations.
          // almost all lines are get in the twi trace.
          if (s[st] != 'g' || s[st + 1] != 'e' || s[st + 2] != 't') {
            skip = 1;
          }
        }
        which++;
        st = k + 1;
      }
    }
    if (which != 7) {
      std::println("strange line {} {} which = {}: {}\n{}", i, j, which,
                   s.substr(i, j - i + 1), s.substr(j + 1, 100));
      continue;
    }
    if (!skip) {
      run_query(h);
    }
    i = j;
  }
}
int main() {
  // TODO: to support other traces, we can make an interface for them.
  namespace fs = std::filesystem;
  to_test.push_back(CacheRunner("FIFO", new FIFO(1 << 14)));
  const fs::path twi_traces{twi_trace_location};
  for (const auto &trace : fs::directory_iterator{twi_traces}) {
    // each trace is ~10gb
    // we'll mmap this file.
    auto size = fs::file_size(trace.path());
    int fd = open(trace.path().c_str(), O_RDONLY);
    assert(fd != -1);
    void *trace_addr = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    assert(trace_addr != MAP_FAILED);
    close(fd);
    std::println("mmapped at {}", trace_addr);
    run_twi_trace(std::string_view{(const char *)trace_addr, size});
    munmap(trace_addr, size);
    break;
  }
}
