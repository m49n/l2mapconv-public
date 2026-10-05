#pragma once
#include "../TestSupport.h"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <functional>

namespace pf_test {
inline int check(const char *name, const std::function<bool()> &test) {
  try { return expect(test(), name); }
  catch (const std::exception &e) { std::cerr << name << ": " << e.what() << '\n'; return 1; }
}
inline bool rejects(const std::function<void()> &f) {
  try { f(); return false; } catch (const std::exception &) { return true; }
}
struct TempDirectory {
  std::filesystem::path path = std::filesystem::temp_directory_path() /
      ("pathfinding-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  TempDirectory() { std::filesystem::create_directories(path); }
  ~TempDirectory() {
    std::error_code error;std::filesystem::remove_all(path,error);
    if(error) std::cerr<<"Test temporary retained: "<<path<<": "<<error.message()<<'\n';
  }
  auto file(const char *name, const char *bytes) const -> std::filesystem::path {
    auto p = path / name;
    std::ofstream(p, std::ios::binary) << bytes;
    return p;
  }
};
}
