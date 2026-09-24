#include "Fixtures.h"
#include "TestSupport.h"
#include <fstream>
#include <iostream>
#include <territory/PathIO.h>
#include <territory/TerritoryRenderer.h>
#include <territory/VisualSceneLoader.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
int gpu_flattened_draw_tests(territory::TerritoryRenderer &);
namespace {
void png(const std::filesystem::path &file, int size,
         const std::vector<std::uint8_t> &data) {
  if (std::filesystem::exists(file))
    throw std::runtime_error("Existing fixture image");
  std::ofstream out(file, std::ios::binary);
  if (!stbi_write_png_to_func(
          [](void *context, void *p, int n) {
            static_cast<std::ofstream *>(context)->write(static_cast<char *>(p),
                                                         n);
          },
          &out, size, size, 3, data.data(), size * 3) ||
      !out)
    throw std::runtime_error("Fixture PNG failed");
}
territory::VisualScene fixture() {
  using namespace territory;
  VisualScene s;
  s.bounds = {65536, 131072, 65664, 131200, -10, 10};
  auto quad = [&](float x, float y, float w, float h, float z, glm::vec4 c,
                  Blend blend, bool water = false) {
    VisualMesh mesh;
    for (glm::vec2 p : {glm::vec2(x, y), glm::vec2(x + w, y),
                        glm::vec2(x + w, y + h), glm::vec2(x, y + h)}) {
      VisualVertex v;
      v.position = {p.x + 65536, p.y + 131072, z};
      v.normal = {0, 0, 1};
      v.uv[0] = {(p.x - x) / w, (p.y - y) / h};
      mesh.vertices.push_back(v);
    }
    mesh.indices = {0, 1, 2, 0, 2, 3};
    RenderMaterial m;
    m.source = "fixture";
    Node n;
    n.value = c;
    m.nodes.push_back(n);
    m.two_sided = true;
    m.blend = blend;
    // The fixture's numeric expectations use unlit color.
    m.unlit = true;
    s.draws.push_back({s.meshes.size(), s.library.materials.size(), 0, 6,
                       glm::mat4(1.f), water, "fixture"});
    s.meshes.push_back(std::move(mesh));
    s.library.materials.push_back(std::move(m));
  };
  quad(0, 0, 128, 128, 0, {0, 1, 0, 1}, Blend::Opaque);
  quad(0, 0, 64, 64, 1, {1, 0, 0, 0}, Blend::Opaque);
  quad(64, 0, 64, 64, 1, {0, 0, 1, .5f}, Blend::Alpha);
  quad(0, 64, 64, 64, 1, {1, 0, 0, .25f}, Blend::Masked);
  quad(64, 64, 64, 64, 1, {1, 1, 1, 1}, Blend::Opaque);
  auto &node = s.library.materials.back().nodes.front();
  node.op = Op::Sample;
  node.texture = 0;
  TextureData t;
  t.source = "sRGB128";
  t.width = t.height = 2;
  t.usage = TextureUsage::Color;
  t.bytes.assign(16, 128);
  for (int i = 3; i < 16; i += 4)
    t.bytes[i] = 255;
  s.library.textures.push_back(t);
  quad(56, 56, 16, 16, 2, {1, 0, 1, 1}, Blend::Opaque, true);
  return s;
}
} // namespace
int gpu_raster_tests(const std::filesystem::path &output) {
  using namespace territory;
  TestDirectory client;
  auto dir = create_job_directory(output, client.path());
  auto s = fixture();
  TerritoryRenderer renderer;
  int failures = 0;
  RasterInfo last_info;
  auto raster = [&](int tiles, bool water) {
    std::vector<std::uint8_t> pixels(256 * 256 * 3);
    int next = 0;
    auto info = renderer.render(
        s, {256, tiles, 4, water}, {}, {},
        [&](int y, int width, int count, std::span<const std::uint8_t> bytes) {
          if (y != next || width != 256 ||
              bytes.size() != static_cast<std::size_t>(width) * count * 3)
            throw std::runtime_error("Invalid row-band order");
          std::copy(bytes.begin(), bytes.end(), pixels.begin() + y * width * 3);
          next += count;
        });
    if (next != 256)
      throw std::runtime_error("Missing rows");
    std::cout << info.gpu << " samples=" << info.samples << '\n';
    last_info = info;
    return pixels;
  };
  auto whole = raster(256, true), tiled = raster(128, true),
       dry = raster(128, false);
  failures += gpu_flattened_draw_tests(renderer);
  {
    auto point = s.draws.front();
    point.source = "zero-scale actor";
    for (int axis = 0; axis < 3; ++axis)
      point.transform[axis] = glm::vec4(0.f);
    point.transform[3] = {65550, 131080, 5, 1};
    s.draws.push_back(point);
    bool recovered = false;
    try {
      recovered = raster(128, true) == whole && last_info.issues.size() == 1 &&
                  last_info.issues[0].kind == IssueKind::Simplified &&
                  last_info.issues[0].source == "zero-scale actor" &&
                  last_info.issues[0].surfaces ==
                      std::vector<std::string>{"zero-scale actor"};
    } catch (const std::exception &e) {
      std::cout << "Zero-scale reproduction: " << e.what() << '\n';
    }
    failures += expect(
        recovered,
        "zero-scale actor reports skip without changing pixels or aborting");
    s.draws.pop_back();
  }
  {
    RenderMaterial unused;
    unused.source = "retained original before UV neutralization";
    Node sample;
    sample.op = Op::Sample;
    sample.texture = 0;
    sample.uv_channel = 4;
    unused.nodes.push_back(sample);
    s.library.materials.push_back(unused);
    bool recovered = false;
    try {
      recovered = raster(128, true) == whole;
    } catch (const std::exception &e) {
      std::cout << "UV fallback reproduction: " << e.what() << '\n';
    }
    failures += expect(
        recovered,
        "unused unsupported UV original does not abort neutralized scene");
    s.library.materials.pop_back();
  }
  std::uint64_t total = 0;
  unsigned maximum = 0;
  for (std::size_t i = 0; i < whole.size(); ++i) {
    auto delta = static_cast<unsigned>(std::abs(int(whole[i]) - int(tiled[i])));
    total += delta;
    maximum = std::max(maximum, delta);
  }
  failures += expect(maximum <= 2, "whole/tiled raster agreement");
  auto pixel = [&](int x, int y, int r, int g, int b) {
    const auto i = (y * 256 + x) * 3;
    return std::abs(int(whole[i]) - r) <= 2 &&
           std::abs(int(whole[i + 1]) - g) <= 2 &&
           std::abs(int(whole[i + 2]) - b) <= 2;
  };
  failures += expect(pixel(32, 32, 255, 0, 0),
                     "opaque alpha=0 must not discard or blend");
  failures += expect(pixel(200, 32, 0, 188, 188),
                     "alpha blending occurs in linear light");
  failures += expect(pixel(32, 200, 0, 255, 0),
                     "masked below cutoff reveals opaque ground");
  failures += expect(pixel(200, 200, 128, 128, 128),
                     "sRGB texture decode and encode occur once");
  failures += expect(whole != dry && pixel(128, 128, 255, 0, 255),
                     "water toggle filters only marked geometry");
  VisualScene land;
  land.bounds = s.bounds;
  land.terrain_mesh = 0;
  land.meshes.push_back(s.meshes[0]);
  for (auto color : {glm::vec4(0, 1, 0, 1), glm::vec4(0, 0, 1, 1)}) {
    RenderMaterial m;
    Node n;
    n.value = color;
    m.nodes.push_back(n);
    m.unlit = true;
    land.library.materials.push_back(m);
  }
  for (auto mask : {255, 128}) {
    TextureData t;
    t.width = t.height = 1;
    t.encoding = PixelEncoding::R8;
    t.usage = TextureUsage::Mask;
    t.bytes = {static_cast<std::uint8_t>(mask)};
    land.library.textures.push_back(t);
  }
  land.terrain_layers = {{0, 0, glm::mat4(1.f)}, {1, 1, glm::mat4(1.f)}};
  std::vector<std::uint8_t> land_pixels(256 * 256 * 3);
  renderer.render(
      land, {256, 128, 4, true}, {}, {},
      [&](int y, int width, int, std::span<const std::uint8_t> bytes) {
        std::copy(bytes.begin(), bytes.end(),
                  land_pixels.begin() + y * width * 3);
      });
  failures += expect(
      std::abs(int(land_pixels[3 * 257 + 1]) - 187) <= 2 &&
          std::abs(int(land_pixels[3 * 257 + 2]) - 188) <= 2,
      "terrain layer masks remain linear data and blend in declared order");
  bool cancelled = false;
  for (float height : {20000.f, -21000.f}) {
    auto high = land;
    for (auto &vertex : high.meshes[0].vertices)
      vertex.position.z = height;
    expand_scene_z_bounds(high);
    std::vector<std::uint8_t> high_pixels(land_pixels.size());
    renderer.render(
        high, {256, 128, 4, true}, {}, {},
        [&](int y, int width, int, std::span<const std::uint8_t> data) {
          std::copy(data.begin(), data.end(),
                    high_pixels.begin() + y * width * 3);
        });
    failures +=
        expect(high_pixels == land_pixels,
               "terrain-only geometry outside legacy Z bounds remains visible");
  }
  {
    auto boundary = [&](int count, bool terrain) {
      VisualScene scene;
      scene.bounds = s.bounds;
      scene.meshes.push_back(s.meshes[0]);
      RenderMaterial material;
      material.source = "sixteen-sampler fixture";
      material.two_sided = true;
      material.unlit = true;
      for (int i = 0; i < count; ++i) {
        TextureData t;
        t.width = t.height = 1;
        t.bytes = {128, 128, 128, 255};
        scene.library.textures.push_back(t);
        Node sample;
        sample.op = Op::Sample;
        sample.texture = i;
        const auto index = static_cast<std::uint32_t>(material.nodes.size());
        material.nodes.push_back(sample);
        if (i) {
          Node sum;
          sum.op = Op::Add;
          sum.inputs = {material.root, index};
          material.nodes.push_back(sum);
        }
        material.root = static_cast<std::uint32_t>(material.nodes.size() - 1);
      }
      Node factor;
      factor.value = glm::vec4(1.f / count);
      const auto factor_id = static_cast<std::uint32_t>(material.nodes.size());
      material.nodes.push_back(factor);
      Node scaled;
      scaled.op = Op::Multiply;
      scaled.inputs = {material.root, factor_id};
      material.nodes.push_back(scaled);
      material.root = static_cast<std::uint32_t>(material.nodes.size() - 1);
      scene.library.materials.push_back(material);
      if (terrain) {
        scene.terrain_mesh = 0;
        TextureData mask;
        mask.width = mask.height = 1;
        mask.encoding = PixelEncoding::R8;
        mask.usage = TextureUsage::Mask;
        mask.bytes = {255};
        scene.library.textures.push_back(mask);
        scene.terrain_layers = {
            {0, static_cast<std::size_t>(count), glm::mat4(1)}};
      } else
        scene.draws = {
            {0, 0, 0, 6, glm::mat4(1), false, "sixteen-sampler surface"}};
      return scene;
    };
    for (const auto [count, terrain] :
         {std::pair{16, false}, std::pair{15, true}, std::pair{16, true}}) {
      bool valid = false;
      try {
        int center = -1;
        auto info = renderer.render(
            boundary(count, terrain), {256, 128, 4, true, 16}, {}, {},
            [&](int y, int width, int rows,
                std::span<const std::uint8_t> data) {
              if (y <= 128 && y + rows > 128)
                center = data[((128 - y) * width + 128) * 3];
            });
        const bool fallback = terrain && count == 16;
        valid = fallback
                    ? info.issues.size() == 1 &&
                          info.issues[0].kind == IssueKind::Unsupported &&
                          center >= 186 && center <= 189
                    : info.issues.empty() && center >= 126 && center <= 130;
      } catch (const std::exception &e) {
        std::cout << "Sampler boundary reproduction: " << e.what() << '\n';
      }
      failures += expect(
          valid,
          terrain ? (count == 16
                         ? "terrain sampler overflow reports neutral fallback"
                         : "15 terrain samplers plus mask fit baseline GPU")
                  : "ordinary 16 sampler material uses entire baseline GPU "
                    "capacity");
    }
  }
  try {
    renderer.render(s, {256, 128, 4, true}, [] { return true; }, {}, {});
  } catch (const Cancelled &) {
    cancelled = true;
  }
  failures += expect(cancelled, "GPU render honors cancellation");
  png(dir / "whole.png", 256, whole);
  png(dir / "tiled.png", 256, tiled);
  png(dir / "water-off.png", 256, dry);
  png(dir / "terrain-mask.png", 256, land_pixels);
  write_json_atomic(dir / "report.json",
                    {{"max_channel_difference", maximum},
                     {"mean_channel_difference", double(total) / whole.size()},
                     {"assertion_failures", failures}},
                    false);
  std::cout << path_utf8(dir) << '\n';
  return failures ? 1 : 0;
}
int gpu_map_test(const std::filesystem::path &client, const std::string &map,
                 const std::filesystem::path &output) {
  using namespace territory;
  auto dir = create_job_directory(output, client);
  auto scene = VisualSceneLoader(client).load(map, {});
  constexpr int size = 4096;
  std::vector<std::uint8_t> pixels(static_cast<std::size_t>(size) * size * 3);
  TerritoryRenderer renderer;
  auto info = renderer.render(
      scene, {size, 2048, 4, true}, {},
      [](std::size_t done, std::size_t total) {
        std::cout << "tile " << done << "/" << total << '\n';
      },
      [&](int y, int width, int, std::span<const std::uint8_t> bytes) {
        std::copy(bytes.begin(), bytes.end(),
                  pixels.begin() + static_cast<std::size_t>(y) * width * 3);
      });
  png(dir / (map + ".png"), size, pixels);
  scene.report.maps.front()["gpu"] = info.gpu;
  write_json_atomic(dir / "report.json", to_json(scene.report, "gpu-map-test"),
                    false);
  std::cout << path_utf8(dir) << '\n';
  return 0;
}
