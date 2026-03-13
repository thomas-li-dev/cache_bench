#pragma once
#include "ext/json.hpp"
#include "types.h"
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <optional>
#include <vector>

namespace fs = std::filesystem;
struct Metadata {
  size_t uniq_keys;
};

class Trace {
private:
  std::string name;
  std::vector<fs::path> blocks;
  size_t nxt_blk{}, max_blocks, working_set_size;

public:
  Trace(const std::string &name, const fs::path &path, size_t max_blocks)
      : name(name), max_blocks(max_blocks) {
    auto meta_path = path / "meta.json";
    std::ifstream meta_file{meta_path};
    nlohmann::json meta;
    meta_file >> meta;
    working_set_size = meta["uniq_keys"];

    for (auto &file : fs::directory_iterator{path}) {
      std::string name = file.path().filename();
      if (name.ends_with(".trace")) {
        blocks.push_back(file.path());
      }
    }
    std::sort(blocks.begin(), blocks.end());
  }
  // put the next block into the buffer
  void next_block(std::vector<cache_key_t> &buf) {
    if (nxt_blk == std::min(blocks.size(), max_blocks)) {
      // signal end
      buf.clear();
      return;
    }
    std::ifstream file;
    file.open(blocks[nxt_blk++], std::ios::binary | std::ios::ate);
    // ate seeks to end, so tellg gives the size.
    size_t size = file.tellg();
    file.seekg(0);
    // TODO: use this for smth
    Metadata meta;
    file.read((char *)&meta, sizeof(Metadata));
    size_t num_queries = (size - sizeof(Metadata)) / sizeof(cache_key_t);
    buf.resize(num_queries);
    file.read((char *)buf.data(), num_queries * sizeof(cache_key_t));
  }
  std::string get_name() { return name; }
  size_t get_working_set_size() { return working_set_size; }
};
