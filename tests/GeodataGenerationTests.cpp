#include "Compressor.h"
#include "L2JSerializer.h"
#include "NSWE.h"
#include "PtsSerializer.h"
#include "TestSupport.h"

#include <geodata/Builder.h>
#include <glm/gtc/matrix_transform.hpp>
#include <utils/Log.h>

#include <array>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace {

void add_floor(geodata::Mesh &mesh, float x0, float x1, float z,
               float normal = 1.0f) {
  const auto base = static_cast<unsigned int>(mesh.vertices.size());
  for (auto p : {glm::vec3{x0, 0, z}, glm::vec3{x1, 0, z},
                 glm::vec3{x1, 256, z}, glm::vec3{x0, 256, z}}) {
    mesh.vertices.push_back({p, {0, 0, normal}});
  }
  for (auto i : {0U, 1U, 2U, 0U, 2U, 3U})
    mesh.indices.push_back(base + i);
}

auto floor_mesh() -> std::shared_ptr<geodata::Mesh> {
  auto mesh = std::make_shared<geodata::Mesh>();
  mesh->instance_matrices.push_back(glm::mat4{1});
  return mesh;
}

auto walkable_span(const rcHeightfield &field, int x, int y) -> const rcSpan * {
  for (auto span = field.spans[x + y * field.width]; span; span = span->next) {
    if (geodata::unpack_area(span->area) != RC_NULL_AREA)
      return span;
  }
  return nullptr;
}

auto check_null_neighbors() -> int {
  auto failures = 0;
  struct Case {
    int x, y, neighbor_x, neighbor_y, bit;
  };
  // E, S, W, N in raw Recast bit order after rotating the same scene.
  const std::array cases{Case{7, 8, 8, 8, 4}, Case{7, 7, 7, 8, 2},
                         Case{8, 7, 7, 7, 1}, Case{8, 8, 8, 7, 8}};
  for (std::size_t i = 0; i < cases.size(); ++i) {
    for (auto neighbor_normal : {-1.0f, 1.0f}) {
      auto mesh = floor_mesh();
      add_floor(*mesh, 0, 127.75f, 0);
      add_floor(*mesh, 128.25f, 256, 0, neighbor_normal);
      auto transform = glm::translate(glm::mat4{1}, glm::vec3{128, 128, 0});
      transform =
          glm::rotate(transform, glm::radians(90.0f * i), glm::vec3{0, 0, 1});
      transform = glm::translate(transform, glm::vec3{-128, -128, 0});
      geodata::Map map{"null-neighbor",
                       geometry::Box{{0, 0, -128}, {256, 256, 512}}};
      map.add({mesh, transform});
      geodata::NSWE nswe{map, 48, 16, 45.5f, 2, 16, 16, 1};
      const auto &field = nswe.calculate_nswe();
      const auto &c = cases[i];
      const auto *source = walkable_span(field, c.x, c.y);
      const auto *destination =
          walkable_span(field, c.neighbor_x, c.neighbor_y);
      failures +=
          expect(source != nullptr, "NSWE fixture has a walkable source");
      const auto valid = neighbor_normal > 0;
      failures +=
          expect((destination != nullptr) == valid,
                 "down-facing neighbor is excluded from exported geometry");
      if (source) {
        failures +=
            expect(((geodata::unpack_nswe(source->area) & c.bit) != 0) == valid,
                   "NSWE permits a real floor, not an excluded neighbor");
      }
    }
  }
  return failures;
}

template <typename Action>
auto rejects(Action action, std::string_view message = {}) -> bool {
  try {
    action();
  } catch (const std::runtime_error &error) {
    return *error.what() != '\0' && std::string_view{error.what()}.find(
                                        message) != std::string_view::npos;
  }
  return false;
}

auto check_export_limits() -> int {
  auto failures = 0;
  geodata::ExportBuffer buffer;
  geodata::Geodata data;
  data.cells.push_back(
      {0, 1, -1000, geodata::BLOCK_MULTILAYER, true, true, true, true});
  for (int i = 0; i < 65; ++i) {
    data.cells.push_back({0, 0, static_cast<std::int16_t>(i * 16),
                          geodata::BLOCK_MULTILAYER, true, true, true, true});
  }
  failures +=
      expect(rejects([&] { buffer.reset(data); }),
             "65th layer fails explicitly before writing into the next column");
  failures += expect(buffer.cell(0, 0, 0, 1).z == -1000,
                     "overflow rejection preserves the adjacent column");
  data.cells.pop_back();
  buffer.reset(data);
  failures +=
      expect(buffer.column(0, 0, 0, 0).layers == 64 &&
                 buffer.cell(0, 0, 0, 0, 63).z == 1008,
             "all 64 supported layers remain available after a rejected reset");

  for (auto height : {20000, -20000}) {
    data.cells.clear();
    for (std::int16_t x = 0; x < 8; ++x) {
      for (std::int16_t y = 0; y < 8; ++y) {
        data.cells.push_back({x, y, static_cast<std::int16_t>(height),
                              geodata::BLOCK_SIMPLE, true, true, true, true});
      }
    }
    buffer.reset(data);
    std::ostringstream l2j, pts;
    geodata::L2JSerializer{}.serialize(buffer, l2j);
    geodata::PtsSerializer{}.serialize(buffer, 24, 18, pts);
    const auto lo = static_cast<char>(height > 0 ? 0x20 : 0xe0);
    const auto hi = static_cast<char>(height > 0 ? 0x4e : 0xb1);
    failures += expect(l2j.str().substr(0, 3) == std::string{'\0', lo, hi},
                       "L2J flat blocks retain raw signed 16-bit heights");
    failures += expect(pts.str().substr(18, 6) ==
                           std::string{'\0', '\0', lo, hi, lo, hi},
                       "PTS flat blocks retain both raw signed 16-bit heights");
    for (auto type : {geodata::BLOCK_COMPLEX, geodata::BLOCK_MULTILAYER}) {
      if (type == geodata::BLOCK_MULTILAYER) {
        data.cells.push_back(
            {7, 7, 0, geodata::BLOCK_SIMPLE, true, true, true, true});
        buffer.reset(data);
        data.cells.pop_back();
      }
      buffer.set_block_type(0, 0, type);
      failures +=
          expect(rejects(
                     [&] {
                       std::ostringstream output;
                       geodata::L2JSerializer{}.serialize(buffer, output);
                     },
                     "outside the packed range"),
                 "L2J rejects packed heights before encoding");
      failures += expect(rejects(
                             [&] {
                               std::ostringstream output;
                               geodata::PtsSerializer{}.serialize(buffer, 24,
                                                                  18, output);
                             },
                             "outside the packed range"),
                         "PTS rejects packed heights before encoding");
    }
    for (auto &cell : data.cells)
      cell.type = geodata::BLOCK_MULTILAYER;
    const auto reset_rejected = rejects([&] { buffer.reset(data); });
    failures +=
        expect(!reset_rejected,
               "uncompressed heights remain available for flat compression");
    if (!reset_rejected) {
      // Compressor visits the entire region and requires nonempty columns,
      // including when assertions are enabled. Complete the sparse fixture.
      data.cells.reserve(2048 * 2048);
      for (std::int16_t x = 0; x < 2048; ++x) {
        for (std::int16_t y = 0; y < 2048; ++y) {
          if (x < 8 && y < 8)
            continue;
          data.cells.push_back({x, y, static_cast<std::int16_t>(height),
                                geodata::BLOCK_MULTILAYER, true, true, true,
                                true});
        }
      }
      buffer.reset(data);
      geodata::Compressor{buffer}.compress();
      failures +=
          expect(buffer.block(0, 0).type == geodata::BLOCK_SIMPLE &&
                     buffer.cell(0, 0).z == height,
                 "high uniform floors compress to valid raw-height blocks");
    }
  }
  data.cells.clear();
  for (std::int16_t x = 0; x < 8; ++x) {
    for (std::int16_t y = 0; y < 8; ++y) {
      data.cells.push_back({x, y, -16384, geodata::BLOCK_MULTILAYER, false,
                            false, false, false});
    }
  }
  data.cells.push_back(
      {0, 0, 16376, geodata::BLOCK_MULTILAYER, true, true, true, true});
  buffer.reset(data);
  failures += expect(buffer.cell(0, 0).z == -16384 &&
                         buffer.cell(0, 0, 0, 0, 1).z == 16376,
                     "packed height endpoints are preserved");
  std::ostringstream l2j, pts;
  geodata::L2JSerializer{}.serialize(buffer, l2j);
  geodata::PtsSerializer{}.serialize(buffer, 24, 18, pts);
  const std::string encoded_endpoints{'\0', static_cast<char>(0x80),
                                      static_cast<char>(0xff), 0x7f};
  failures += expect(
      l2j.str().substr(0, 6) == std::string{'\2', '\2'} + encoded_endpoints,
      "L2J encodes both packed-height endpoints without overflow");
  failures +=
      expect(pts.str().substr(18, 8) ==
                 std::string{0x41, '\0', '\2', '\0'} + encoded_endpoints,
             "PTS encodes both packed-height endpoints without overflow");
  return failures;
}

auto check_builder_height_narrowing() -> int {
  auto mesh = floor_mesh();
  add_floor(*mesh, 0, 256, 40000);
  geodata::Map map{"height-overflow",
                   geometry::Box{{0, 0, 20000}, {256, 256, 50000}}};
  map.add({mesh, glm::mat4{1}});
  geodata::Builder builder;
  const geodata::BuilderSettings settings{48, 16, 45.5f, 2, 16, 16, 1};
  return expect(rejects([&] { builder.build(map, settings); },
                        "in map height-overflow at cell"),
                "builder rejects world heights before narrowing to int16");
}

} // namespace

auto run_geodata_generation_tests() -> int {
  const auto previous_level = utils::Log::level;
  utils::Log::level = utils::LOG_NONE;
  auto failures = check_null_neighbors();
  failures += check_export_limits();
  failures += check_builder_height_narrowing();
  utils::Log::level = previous_level;
  return failures;
}
