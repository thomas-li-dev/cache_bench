#pragma once
#include "types.h"
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <optional>
#include <print>
#include <vector>

namespace fs = std::filesystem;
struct Metadata {
  size_t uniq_keys;
};

class Trace {
private:
  std::vector<fs::path> blocks;
  size_t nxt_blk{};
  std::string name;

public:
  Trace(const std::string &name, const fs::path &path) : name(name) {
    for (auto &file : fs::directory_iterator{path}) {
      std::string name = file.path().filename();
      if (name.ends_with(".trace")) {
        std::println("adding {}", name);
        blocks.push_back(file.path());
      }
    }
    std::sort(blocks.begin(), blocks.end());
  }
  // put the next block into the buffer
  void next_block(std::vector<cache_key_t> &buf) {
    if (nxt_blk == blocks.size()) {
      // signal end
      buf.clear();
      return;
    }
    std::ifstream file;
    file.open(blocks[nxt_blk++], std::ios::binary | std::ios::ate);
    // ate seeks to end, so tellg gives the size.
    size_t size = file.tellg();
    file.seekg(0);
    Metadata meta;
    file.read((char *)&meta, sizeof(Metadata));
    size_t num_queries = (size - sizeof(Metadata)) / sizeof(cache_key_t);
    buf.resize(num_queries);
    file.read((char *)buf.data(), num_queries * sizeof(cache_key_t));
  }
  std::string get_name() { return name; }
};
