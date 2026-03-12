#include "itrace.h"
#include <cassert>
#include <fcntl.h>
#include <filesystem>
#include <print>
#include <sys/mman.h>
#include <unistd.h>

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
namespace fs = std::filesystem;
class TwiTrace : public ITrace {
private:
  const fs::path path;
  std::string_view current_trace_suffix;
  std::vector<fs::directory_entry> files;
  size_t next_file = 0;

  // these are just used for unmapping.
  void *current_trace_addr = nullptr;
  size_t current_trace_size = 0;
  void *mmap_trace_part(const fs::directory_entry &trace) {
    auto size = fs::file_size(trace.path());
    int fd = open(trace.path().c_str(), O_RDONLY);
    assert(fd != -1);
    void *trace_addr = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    assert(trace_addr != MAP_FAILED);
    close(fd);
    return trace_addr;
  }
  std::optional<cache_key_t> parse_line(std::string_view s) {
    int which = 0;

    uint64_t h = 0;
    bool skip = 0;

    for (size_t k = 0, st = 0; k <= s.size(); k++) {
      if (k == s.size() || s[k] == ',') {
        std::string_view t = s.substr(st, k - st);
        if (which == 1) {
          // compute a simple rolling hash of the string.
          // trivial to hack but should be fine for the trace.
          h = 0;
          for (char c : t)
            h = h * 31 + c;
        } else if (which == 5) {
          // skip none get / gets operations.
          // almost all lines are get in the twi trace.
          if (!t.starts_with("get"))
            skip = 1;
        }
        which++;
        st = k + 1;
      }
    }
    if (which != 7 || skip)
      return {};
    return h;
  };
  std::optional<cache_key_t> next_key() override {
    while (true) {
      if (current_trace_suffix.empty()) {
        if (next_file == files.size())
          return {};
        if (current_trace_addr)
          munmap(current_trace_addr, current_trace_size);

        auto next_trace = files[next_file++];
        current_trace_addr = mmap_trace_part(next_trace);
        current_trace_size = next_trace.file_size();
        current_trace_suffix = {(const char *)current_trace_addr,
                                next_trace.file_size()};
      }
      auto end = current_trace_suffix.find_first_of('\n');
      if (end == std::string_view::npos) {
        // skip to the next trace.
        current_trace_suffix = {};
        continue;
      }
      std::string_view line = current_trace_suffix.substr(0, end);
      current_trace_suffix.remove_prefix(end + 1);
      auto k = parse_line(line);
      if (k) {
        return k;
      }
    }
  }

public:
  // generally we assume that the path contains the whole trace files.
  // don't do much error checking.
  TwiTrace(const fs::path &path) : path(path) {
    for (auto &file : fs::directory_iterator{path}) {
      files.push_back(file);
    }
    std::sort(files.begin(), files.end());
  }
  ~TwiTrace() {
    if (current_trace_addr)
      munmap(current_trace_addr, current_trace_size);
  }

  void reset() override {
    if (current_trace_addr)
      munmap(current_trace_addr, current_trace_size);
    current_trace_addr = nullptr;
    current_trace_size = 0;
    current_trace_suffix = {};
    next_file = 0;
  }
  // similar interface to read syscall
  size_t next_buf(std::vector<cache_key_t> &buf) override {
    for (size_t i = 0; i < buf.size(); i++) {
      auto k = next_key();
      if (k) {
        buf[i] = *k;
      } else
        return i;
    }
    return buf.size();
  }
};