#pragma once
#include "Job.h"
#include <filesystem>
#include <memory>
#include <span>
namespace territory {
class PngOutput {
public:
  // Test-only failure injection at the write/commit boundary; no filesystem
  // bypass.
  using Checkpoint = std::function<void(std::string_view)>;
  PngOutput(const std::filesystem::path &final_path, int width, int height,
            Checkpoint = {});
  ~PngOutput();
  PngOutput(const PngOutput &) = delete;
  void rows(int first_y, int width, int count, std::span<const std::uint8_t>);
  void finish(const Cancel &);

private:
  struct Impl;
  std::unique_ptr<Impl> impl;
};
bool png_has_dimensions(const std::filesystem::path &, int width, int height);
} // namespace territory
