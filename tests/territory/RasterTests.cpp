#include "Fixtures.h"
#include "TestSupport.h"
#include <limits>
#include <territory/TileLayout.h>
int raster_tests() {
  using namespace territory;
  int failures = 0;
  Bounds b{65536, 131072, 98304, 163840, -10000, 10000};
  auto ts = tile_layout(b, 16384, 2048);
  failures += expect(ts.size() == 64, "16K uses 64 bounded tiles");
  if (!ts.empty())
    failures += expect(ts.front().x == 0 && ts.back().x == 14336 &&
                           ts.back().y == 14336,
                       "coverage endpoints");
  failures +=
      expect(world_to_pixel(b, 8192, {65536, 131072}) == glm::dvec2(0, 0),
             "NW origin");
  failures +=
      expect(world_to_pixel(b, 8192, {98304, 163840}) == glm::dvec2(8192, 8192),
             "SE edge");
  failures +=
      expect(world_to_pixel(b, 8192, {65538, 131074}) == glm::dvec2(.5, .5),
             "pixel centers");
  b = {-32768, -65536, 0, -32768, -10, 10};
  failures +=
      expect(world_to_pixel(b, 128, {-16384, -49152}) == glm::dvec2(64, 64),
             "negative world coordinates");
  ts = tile_layout(b, 130, 128);
  failures +=
      expect(ts.size() == 4 && ts.back().width == 2 && ts.back().height == 2,
             "partial edge tile");
  std::size_t area = 0;
  for (const auto &t : ts)
    area += t.width * t.height;
  failures += expect(area == 130 * 130, "coverage without gaps or overlap");
  failures += expect(
      throws([&] { tile_layout(b, std::numeric_limits<int>::max(), 2048); }),
      "multiplication overflow rejected");
  failures += expect(throws([&] { tile_layout(b, 256, 0); }),
                     "zero tile size rejected");
  b.max_x = b.min_x;
  failures += expect(throws([&] { tile_layout(b, 256, 128); }),
                     "empty world extent rejected");
  b.max_x = std::numeric_limits<double>::infinity();
  failures += expect(throws([&] { world_to_pixel(b, 256, {0, 0}); }),
                     "nonfinite bounds rejected");
  b = {0, 0, 128, 128, 0, 0};
  VisualScene flat;
  flat.bounds = b;
  failures += expect(!throws([&] { validate_scene(flat); }) &&
                         tile_layout(b, 128, 128).front().world.min_z < 0,
                     "flat scene receives nonzero depth margin");
  return failures;
}
