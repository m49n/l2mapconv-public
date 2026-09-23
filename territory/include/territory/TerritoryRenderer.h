#pragma once
#include "Job.h"
#include "VisualScene.h"
#include <functional>
#include <memory>
#include <span>
namespace territory {
struct RasterSettings {
  int pixels{8192}, tile_size{2048}, samples{4};
  bool water{true};
  // Optional lower capability ceiling for deterministic GPU boundary tests.
  int max_texture_units{};
};
using Rows = std::function<void(int first_y, int width, int count,
                                std::span<const std::uint8_t>)>;
using TileProgress = std::function<void(std::size_t done, std::size_t total)>;
struct RasterInfo {
  std::string gpu;
  int samples{};
  std::vector<Issue> issues{};
};
class TerritoryRenderer {
public:
  TerritoryRenderer();
  ~TerritoryRenderer();
  TerritoryRenderer(const TerritoryRenderer &) = delete;
  RasterInfo render(const VisualScene &, const RasterSettings &, const Cancel &,
                    const TileProgress &, const Rows &);

private:
  struct Impl;
  std::unique_ptr<Impl> impl;
};
} // namespace territory
