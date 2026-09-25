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
  auto raster_with_settings = [&](const VisualScene &source,
                                  const RasterSettings &options,
                                  RasterInfo *result = nullptr) {
    std::vector<std::uint8_t> pixels(256 * 256 * 3);
    auto info = renderer.render(
        source, options, {}, {},
        [&](int y, int width, int, std::span<const std::uint8_t> bytes) {
          std::copy(bytes.begin(), bytes.end(),
                    pixels.begin() + static_cast<std::size_t>(y) * width * 3);
        });
    if (result)
      *result = std::move(info);
    return pixels;
  };
  RasterSettings appearance{256, 128, 4, true};
  auto explicit_textured = raster_with_settings(s, appearance);
  failures += expect(explicit_textured == tiled,
                     "explicit texture-on setting keeps original pixels");
  appearance.textures = false;
  auto flat = raster_with_settings(s, appearance);
  failures += expect(flat != explicit_textured,
                     "texture-off changes material color");
  auto without_masked = s;
  without_masked.draws.erase(without_masked.draws.begin() + 3);
  auto flat_without_masked = raster_with_settings(without_masked, appearance);
  const auto hole = (200 * 256 + 32) * 3;
  failures += expect(std::equal(flat.begin() + hole, flat.begin() + hole + 3,
                                flat_without_masked.begin() + hole),
                     "texture-off preserves masked cutout hole");
  appearance.water = false;
  auto flat_dry = raster_with_settings(s, appearance);
  failures += expect(flat_dry != flat,
                     "water switch still removes water when textures hidden");
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
  appearance.water = true;
  auto flat_terrain = raster_with_settings(land, appearance);
  auto zero_masks = land;
  for (auto &mask : zero_masks.library.textures)
    mask.bytes[0] = 0;
  auto flat_zero_masks = raster_with_settings(zero_masks, appearance);
  auto base_only = land;
  base_only.terrain_layers.clear();
  auto flat_base_only = raster_with_settings(base_only, appearance);
  failures += expect(flat_terrain != flat_zero_masks &&
                         flat_zero_masks == flat_base_only,
                     "texture-off still honors terrain layer coverage masks");
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
  {
    VisualScene shadow_scene;
    shadow_scene.bounds = {65536, 131072, 65664, 131200, -1, 25};
    auto add_material = [&](glm::vec4 color, Blend blend = Blend::Opaque) {
      RenderMaterial material;
      material.source = "shadow fixture";
      material.blend = blend;
      material.two_sided = true;
      material.unlit = true;
      Node node;
      node.value = color;
      material.nodes.push_back(node);
      shadow_scene.library.materials.push_back(std::move(material));
      return shadow_scene.library.materials.size() - 1;
    };
    auto add_quad = [&](VisualScene &target, float x0, float y0, float x1,
                        float y1, float z,
                        std::size_t material, bool water = false) {
      VisualMesh mesh;
      for (auto [x, y, u, v] :
           {std::array{x0, y0, 0.f, 0.f}, std::array{x1, y0, 1.f, 0.f},
            std::array{x1, y1, 1.f, 1.f}, std::array{x0, y1, 0.f, 1.f}}) {
        VisualVertex vertex;
        vertex.position = {65536 + x, 131072 + y, z};
        vertex.normal = {0, 0, 1};
        vertex.uv[0] = {u, v};
        mesh.vertices.push_back(vertex);
      }
      mesh.indices = {0, 1, 2, 0, 2, 3};
      target.draws.push_back({target.meshes.size(), material, 0, 6,
                              glm::mat4(1), water, "shadow fixture"});
      target.meshes.push_back(std::move(mesh));
    };
    const auto floor_material = add_material({.8f, .8f, .8f, 1});
    const auto roof_material = add_material({.9f, .9f, .9f, 1});
    add_quad(shadow_scene, 0, 0, 128, 128, 0, floor_material);
    add_quad(shadow_scene, 48, 48, 80, 80, 24, roof_material);
    auto sample_red = [](const auto &image, int x, int y) {
      return int(image[(static_cast<std::size_t>(y) * 256 + x) * 3]);
    };
    auto max_channel_delta = [](const auto &left, const auto &right) {
      unsigned largest = 0;
      for (std::size_t i = 0; i < left.size(); ++i)
        largest = std::max(
            largest, static_cast<unsigned>(std::abs(int(left[i]) - int(right[i]))));
      return largest;
    };
    RasterSettings sunlight{256, 128, 4, true};
    const auto no_shadow = raster_with_settings(shadow_scene, sunlight);
    sunlight.shadows = true;
    sunlight.sun_azimuth_deg = 315;
    sunlight.sun_elevation_deg = 40;
    const auto nw_sun = raster_with_settings(shadow_scene, sunlight);
    failures += expect(sample_red(nw_sun, 180, 180) + 20 <
                           sample_red(no_shadow, 180, 180) &&
                           std::abs(sample_red(nw_sun, 64, 64) -
                                    sample_red(no_shadow, 64, 64)) <= 2,
                       "northwest sun shadows southeast floor only");
    failures += expect(std::abs(sample_red(nw_sun, 128, 128) -
                                sample_red(no_shadow, 128, 128)) <= 2,
                       "raised lit roof has no self-shadow acne");
    bool clear_floor = true;
    for (int y : {16, 32, 48, 64, 80})
      for (int x : {16, 32, 48, 64, 80})
        clear_floor &= std::abs(sample_red(nw_sun, x, y) -
                                sample_red(no_shadow, x, y)) <= 2;
    failures += expect(clear_floor,
                       "unoccluded floor has no shadow-map acne");
    sunlight.tile_size = 256;
    const auto one_tile = raster_with_settings(shadow_scene, sunlight);
    failures += expect(max_channel_delta(nw_sun, one_tile) <= 2,
                       "one global depth map has no color tile seam");
    {
      auto seam_scene = shadow_scene;
      for (auto &vertex : seam_scene.meshes.back().vertices)
        vertex.position.x -= 8;
      auto seam_pixels = [&](const RasterSettings &options) {
        std::array<int, 2> red{-1, -1};
        renderer.render(
            seam_scene, options, {}, {},
            [&](int y, int width, int count,
                std::span<const std::uint8_t> bytes) {
              if (y <= 2560 && y + count > 2560)
                for (int i = 0; i < 2; ++i)
                  red[i] = bytes[((2560 - y) * width +
                                  (i ? 2052 : 2044)) * 3];
            });
        return red;
      };
      RasterSettings large{4096, 2048, 1, true};
      large.shadows = true;
      large.sun_azimuth_deg = 315;
      large.sun_elevation_deg = 40;
      const auto tiled_seam = seam_pixels(large);
      large.tile_size = 1024;
      const auto alternate_seam = seam_pixels(large);
      large.shadows = false;
      const auto unshadowed_seam = seam_pixels(large);
      failures += expect(tiled_seam[0] + 20 < unshadowed_seam[0] &&
                             tiled_seam[1] + 20 < unshadowed_seam[1] &&
                             std::abs(tiled_seam[0] - alternate_seam[0]) <= 2 &&
                             std::abs(tiled_seam[1] - alternate_seam[1]) <= 2,
                         "cast shadow crosses actual 2048-pixel tile seam");
    }
    {
      auto one_sided = shadow_scene;
      one_sided.library.materials[roof_material].two_sided = false;
      const auto ordinary = raster_with_settings(one_sided, sunlight);
      failures += expect(sample_red(ordinary, 180, 180) + 20 <
                             sample_red(no_shadow, 180, 180),
                         "single-sided upward roof casts a shadow");
      auto &mirror = one_sided.draws.back().transform;
      mirror[0][0] = -1;
      mirror[3][0] = 2.f * (65536.f + 64.f);
      const auto mirrored = raster_with_settings(one_sided, sunlight);
      failures += expect(sample_red(mirrored, 180, 180) + 20 <
                             sample_red(no_shadow, 180, 180),
                         "mirrored single-sided roof casts a shadow");
    }
    {
      auto exterior = shadow_scene;
      exterior.draws.pop_back();
      add_quad(exterior, -20, -20, -12, -12, 24, roof_material);
      const auto edge = raster_with_settings(exterior, sunlight);
      failures += expect(sample_red(edge, 8, 8) + 20 <
                             sample_red(no_shadow, 8, 8),
                         "exterior caster shadows receiver inside output square");
    }
    {
      auto unsupported_cutout = shadow_scene;
      auto &roof = unsupported_cutout.library.materials[roof_material];
      roof.blend = Blend::Masked;
      roof.shadow_coverage_reliable = false;
      RasterInfo omission_info;
      const auto omitted =
          raster_with_settings(unsupported_cutout, sunlight, &omission_info);
      failures += expect(std::abs(sample_red(omitted, 180, 180) -
                                   sample_red(no_shadow, 180, 180)) <= 2 &&
                             std::any_of(omission_info.issues.begin(),
                                         omission_info.issues.end(),
                                         [](const Issue &issue) {
                                           return issue.kind == IssueKind::Simplified &&
                                                  issue.reason.find("Shadow caster omitted") !=
                                                      std::string::npos;
                                         }),
                         "unsupported cutout is omitted and reported");
      auto no_texture = sunlight;
      no_texture.textures = false;
      const auto flat_omitted =
          raster_with_settings(unsupported_cutout, no_texture);
      auto flat_bare = unsupported_cutout;
      flat_bare.draws.pop_back();
      const auto flat_without_caster =
          raster_with_settings(flat_bare, no_texture);
      failures += expect(std::abs(sample_red(flat_omitted, 180, 180) -
                                   sample_red(flat_without_caster, 180, 180)) <= 2,
                         "texture-off still omits unsupported cutout caster");
    }
    sunlight.tile_size = 128;
    sunlight.sun_azimuth_deg = 135;
    const auto se_sun = raster_with_settings(shadow_scene, sunlight);
    failures += expect(sample_red(se_sun, 64, 64) + 20 <
                           sample_red(no_shadow, 64, 64) &&
                           std::abs(sample_red(se_sun, 180, 180) -
                                    sample_red(no_shadow, 180, 180)) <= 2,
                       "reversing sun direction reverses cast shadow");
    sunlight.sun_azimuth_deg = 315;
    sunlight.sun_elevation_deg = 15;
    const auto low_sun = raster_with_settings(shadow_scene, sunlight);
    failures += expect(sample_red(low_sun, 240, 240) + 20 <
                           sample_red(nw_sun, 240, 240),
                       "lower sun casts a longer shadow");
    sunlight.sun_elevation_deg = 40;
    {
      auto flat_unshadowed = sunlight;
      flat_unshadowed.shadows = false;
      flat_unshadowed.textures = false;
      auto flat_shadowed = sunlight;
      flat_shadowed.textures = false;
      const auto plain = raster_with_settings(shadow_scene, flat_unshadowed);
      const auto shaded = raster_with_settings(shadow_scene, flat_shadowed);
      failures += expect(sample_red(shaded, 180, 180) + 20 <
                             sample_red(plain, 180, 180),
                         "neutral materials still receive cast shadows");
    }
    {
      auto masked_scene = shadow_scene;
      TextureData checker;
      checker.source = "alpha-checker";
      checker.width = checker.height = 16;
      checker.bytes.resize(16 * 16 * 4, 255);
      for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 8; ++x)
          checker.bytes[(y * 16 + x) * 4 + 3] = 0;
      masked_scene.library.textures.push_back(std::move(checker));
      auto &roof = masked_scene.library.materials[roof_material];
      roof.blend = Blend::Masked;
      roof.nodes.clear();
      Node checker_node;
      checker_node.op = Op::Sample;
      checker_node.texture = 0;
      roof.nodes.push_back(checker_node);
      const auto cutout = raster_with_settings(masked_scene, sunlight);
      failures += expect(
          std::abs(sample_red(cutout, 150, 190) -
                   sample_red(no_shadow, 150, 190)) <= 2 &&
              sample_red(cutout, 190, 190) + 20 <
                  sample_red(no_shadow, 190, 190),
          "masked caster shadows opaque texels but not cutout holes");
      auto limited = sunlight;
      limited.max_texture_units = 1;
      failures += expect(throws([&] {
                           (void)raster_with_settings(masked_scene, limited);
                         }),
                         "shadow sampler reservation fails when GPU units run out");
      png(dir / "shadow-cutout.png", 256, cutout);
    }
    {
      auto bare = shadow_scene;
      bare.draws.pop_back();
      const auto bare_floor = raster_with_settings(bare, sunlight);
      auto translucent = shadow_scene;
      translucent.library.materials[roof_material].blend = Blend::Alpha;
      translucent.library.materials[roof_material].nodes[0].value.a = .5f;
      const auto transparent_roof = raster_with_settings(translucent, sunlight);
      failures += expect(std::abs(sample_red(transparent_roof, 180, 180) -
                                  sample_red(bare_floor, 180, 180)) <= 2,
                         "translucent surfaces do not cast solid shadows");
      auto water_caster = shadow_scene;
      water_caster.draws.back().water = true;
      const auto water_roof = raster_with_settings(water_caster, sunlight);
      failures += expect(std::abs(sample_red(water_roof, 180, 180) -
                                  sample_red(bare_floor, 180, 180)) <= 2,
                         "water geometry does not cast solid shadows");
      auto water_receiver = shadow_scene;
      RenderMaterial water_material;
      water_material.source = "water receiver";
      water_material.unlit = true;
      water_material.two_sided = true;
      Node blue;
      blue.value = {.2f, .4f, .7f, 1};
      water_material.nodes.push_back(blue);
      water_receiver.library.materials.push_back(water_material);
      add_quad(water_receiver, 84, 84, 108, 108, 1,
               water_receiver.library.materials.size() - 1, true);
      auto water_unshadowed_settings = sunlight;
      water_unshadowed_settings.shadows = false;
      const auto water_unshadowed =
          raster_with_settings(water_receiver, water_unshadowed_settings);
      const auto water_shadowed = raster_with_settings(water_receiver, sunlight);
      failures += expect(sample_red(water_shadowed, 180, 180) + 10 <
                             sample_red(water_unshadowed, 180, 180),
                         "water may receive shadows while not casting them");
      png(dir / "shadow-water.png", 256, water_shadowed);
    }
    {
      std::vector<std::pair<std::size_t, std::size_t>> stages;
      const auto info = renderer.render(
          shadow_scene, sunlight, {},
          [&](std::size_t done, std::size_t total) {
            stages.emplace_back(done, total);
          },
          [](int, int, int, std::span<const std::uint8_t>) {});
      failures += expect(info.shadow_map_size == 256 && stages.size() == 5 &&
                             stages.front() == std::pair<std::size_t,
                                                          std::size_t>{1, 5} &&
                             stages.back() == std::pair<std::size_t,
                                                         std::size_t>{5, 5},
                         "shadow pass reports actual size and monotonic progress");
      std::size_t checks = 0, checks_before_color = 0;
      renderer.render(
          shadow_scene, sunlight, [&] {
            ++checks;
            return false;
          },
          [&](std::size_t done, std::size_t) {
            if (done == 1)
              checks_before_color = checks;
          },
          [](int, int, int, std::span<const std::uint8_t>) {});
      bool interrupted_depth = false;
      std::size_t second_checks = 0;
      try {
        renderer.render(
            shadow_scene, sunlight,
            [&] { return ++second_checks >= checks_before_color - 1; }, {},
            [](int, int, int, std::span<const std::uint8_t>) {});
      } catch (const Cancelled &) {
        interrupted_depth = true;
      }
      failures += expect(checks_before_color > 2 && interrupted_depth &&
                             second_checks < checks_before_color,
                         "cancellation interrupts the depth pass before color tiles");
    }
    sunlight.max_shadow_texture_size = 64;
    failures += expect(throws([&] {
                         (void)raster_with_settings(shadow_scene, sunlight);
                       }),
                       "shadow capacity failure does not render without shadows");
    png(dir / "shadow-off.png", 256, no_shadow);
    png(dir / "shadow-northwest.png", 256, nw_sun);
    png(dir / "shadow-southeast.png", 256, se_sun);
    png(dir / "shadow-low.png", 256, low_sun);
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
  png(dir / "textures-off.png", 256, flat);
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
int gpu_shadow_smoke_tests() {
  using namespace territory;
  TerritoryRenderer renderer;
  const auto scene = fixture();
  int failures = 0;
  for (int pixels : {8192, 16384}) {
    RasterSettings settings{pixels, 2048, 1, true};
    settings.shadows = true;
    int next_row = 0;
    std::size_t last_done = 0, last_total = 0;
    const auto info = renderer.render(
        scene, settings, {},
        [&](std::size_t done, std::size_t total) {
          if (done < last_done || done > total)
            throw std::runtime_error("Nonmonotonic large shadow progress");
          last_done = done;
          last_total = total;
        },
        [&](int first_y, int width, int count,
            std::span<const std::uint8_t> bytes) {
          if (first_y != next_row || width != pixels ||
              bytes.size() != static_cast<std::size_t>(width) * count * 3)
            throw std::runtime_error("Invalid large shadow row band");
          next_row += count;
        });
    const auto side = static_cast<std::size_t>(pixels / settings.tile_size);
    failures += expect(info.shadow_map_size == 4096 && next_row == pixels &&
                           last_done == side * side + 1 &&
                           last_total == last_done,
                       "8K/16K color output uses bounded global shadow map");
    std::cout << pixels << "px shadow smoke: " << last_done << "/"
              << last_total << " passes, shadow map " << info.shadow_map_size
              << "px\n";
  }
  return failures ? 1 : 0;
}
