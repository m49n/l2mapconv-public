#include "../navmesh/src/BuildLimits.h"
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <navmesh/Navmesh.h>
#include <stdexcept>

namespace {
void require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
struct Fixture {
  std::shared_ptr<geodata::Mesh> mesh = std::make_shared<geodata::Mesh>();
  Fixture() { mesh->instance_matrices.push_back(glm::mat4{1}); }
  void quad(std::array<glm::vec3, 4> v, glm::vec3 normal) {
    const auto base = static_cast<unsigned>(mesh->vertices.size());
    for (auto p : v)
      mesh->vertices.push_back({p, normal});
    for (auto i : {0U, 1U, 2U, 0U, 2U, 3U})
      mesh->indices.push_back(base + i);
  }
  void floor(float x0, float y0, float x1, float y1, float z = 0) {
    quad({{{x0, y0, z}, {x1, y0, z}, {x1, y1, z}, {x0, y1, z}}}, {0, 0, 1});
  }
  void wall(float x, float y0, float y1) {
    quad({{{x, y0, -16}, {x, y1, -16}, {x, y1, 192}, {x, y0, 192}}}, {1, 0, 0});
  }
  geodata::Map map(glm::vec3 lo = {0, 0, -16}, glm::vec3 hi = {256, 256, 512}) {
    geodata::Map m{"fixture", geometry::Box{lo, hi}};
    m.add({mesh, glm::mat4{1}});
    return m;
  }
};
template <class F> void rejects(F f) {
  bool rejected = false;
  try {
    f();
  } catch (const std::exception &) {
    rejected = true;
  }
  require(rejected, "invalid input must be rejected");
}
struct Directory {
  std::filesystem::path path =
      std::filesystem::temp_directory_path() /
      ("navmesh-tests-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  Directory() { std::filesystem::create_directories(path); }
  ~Directory() {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
  }
};
} // namespace

int main(int argc, char **argv) {
  if ((argc == 3 || argc == 9) && std::string_view(argv[1]) == "--inspect") {
    try {
      const auto mesh = navmesh::load(argv[2]);
      std::size_t tiles = 0, polys = 0;
      for (int i = 0; i < mesh->getMaxTiles(); ++i) {
        const auto *t = static_cast<const dtNavMesh &>(*mesh).getTile(i);
        if (t->header) {
          ++tiles;
          polys += t->header->polyCount;
        }
      }
      std::cout << "tiles=" << tiles << " polygons=" << polys << '\n';
      if (argc == 9)
        std::cout << "reachable=" << std::boolalpha
                  << navmesh::reachable(*mesh,
                                        {std::stof(argv[3]), std::stof(argv[4]),
                                         std::stof(argv[5])},
                                        {std::stof(argv[6]), std::stof(argv[7]),
                                         std::stof(argv[8])})
                  << '\n';
      return 0;
    } catch (const std::exception &e) {
      std::cerr << e.what() << '\n';
      return 1;
    }
  }
  int failures = 0;
  auto test = [&](const char *name, auto f) {
    try {
      f();
      std::cout << "PASS " << name << '\n';
    } catch (const std::exception &e) {
      ++failures;
      std::cerr << "FAIL " << name << ": " << e.what() << '\n';
    }
  };
  test("context includes triangle with all vertices outside bounds", [] {
    Fixture f;
    f.floor(-100, -100, 400, 400);
    navmesh::InputGeometry input{geometry::Box{{0, 0, -16}, {256, 256, 512}}};
    navmesh::append_context(input, f.map(), 64);
    require(input.indices.size() == 6, "crossing triangles dropped");
    Fixture distant;
    distant.floor(1000, 1000, 2000, 2000);
    navmesh::append_context(input, distant.map(), 64);
    require(input.indices.size() == 6, "unrelated geometry retained");
  });
  test("context does not export neighbor tiles and influences edge walkability", [] {
    Fixture f;
    f.floor(0, 0, 1024, 1024);
    auto own = f.map({0, 0, -16}, {1024, 1024, 512});
    auto input = navmesh::input_geometry(own);
    Fixture neighbor;
    neighbor.floor(1024, 0, 2048, 1024);
    // A neighbor-owned wall actually crosses our boundary strip.
    neighbor.wall(960, 0, 1024);
    require(navmesh::context_padding({}) == 64, "wrong border padding");
    navmesh::append_context(input, neighbor.map(), 64);
    auto r = navmesh::build(input, {});
    require(r.tiles == 1, "neighbor tile exported");
    require(!navmesh::reachable(*r.mesh, {944, 512, 1}, {992, 512, 1}),
            "neighbor wall ignored");
    auto isolated = navmesh::build(own, {});
    require(navmesh::reachable(*isolated.mesh, {944, 512, 1}, {976, 512, 1}),
            "fixture has no baseline path");
  });
  test("region grid rejects partial tile ownership", [] {
    navmesh::validate_region_grid({});
    auto s = navmesh::Settings{};
    s.cell_size = 16.1f;
    rejects([&] { navmesh::validate_region_grid(s); });
  });
  test("legacy input overload preserves isolated build", [] {
    Fixture f;
    f.floor(-1024, 0, 1024, 1024);
    auto map = f.map({-1024, 0, -16}, {1024, 1024, 512});
    auto old = navmesh::build(map, {});
    auto input = navmesh::input_geometry(map);
    auto fresh = navmesh::build(input, {});
    Directory temp;
    navmesh::save(*old.mesh, temp.path / "old.navmesh");
    navmesh::save(*fresh.mesh, temp.path / "new.navmesh");
    std::ifstream a(temp.path / "old.navmesh", std::ios::binary);
    std::ifstream b(temp.path / "new.navmesh", std::ios::binary);
    require(std::vector<char>{std::istreambuf_iterator<char>(a), {}} ==
                std::vector<char>{std::istreambuf_iterator<char>(b), {}},
            "isolated build bytes changed");
    rejects([&] { navmesh::append_context(input, map, 64, [] { return true; }); });
  });
  test("flat floor crosses internal tile seams and reports progress", [] {
    Fixture f;
    f.floor(0, 0, 2048, 2048);
    auto map = f.map({0, 0, -16}, {2048, 2048, 512});
    std::size_t done = 0, total = 0;
    auto r = navmesh::build(map, {}, {}, [&](auto d, auto t) {
      done = d;
      total = t;
    });
    require(r.mesh && r.tiles == 4 && done == 4 && total == 4,
            "four tiles must be built");
    require(navmesh::reachable(*r.mesh, {128, 128, 1}, {1920, 1920, 1}),
            "path crosses seam");
  });
  test("wall blocks path but opening permits detour", [] {
    for (bool opening : {false, true}) {
      Fixture f;
      f.floor(0, 0, 256, 256);
      f.wall(128, 0, opening ? 144 : 256);
      auto r = navmesh::build(f.map(), {});
      require(navmesh::reachable(*r.mesh, {64, 96, 1}, {192, 96, 1}) == opening,
              "wall or detour classification is wrong");
    }
  });
  test("passage narrower than actor diameter is eroded", [] {
    Fixture f;
    f.floor(0, 0, 256, 256);
    f.wall(112, 0, 256);
    f.wall(136, 0, 256);
    auto r = navmesh::build(f.map(), {});
    require(!navmesh::reachable(*r.mesh, {124, 64, 1}, {124, 192, 1}),
            "narrow passage remains open");
  });
  test("small step connects but high ledge does not", [] {
    for (float h : {8.f, 64.f}) {
      Fixture f;
      f.floor(0, 0, 128, 256);
      f.floor(128, 0, 256, 256, h);
      auto r = navmesh::build(f.map(), {});
      require(navmesh::reachable(*r.mesh, {64, 128, 1}, {192, 128, h + 1}) ==
                  (h == 8),
              "incorrect climb");
      require(navmesh::reachable(*r.mesh, {192, 128, h + 1}, {64, 128, 1}) ==
                  (h == 8),
              "no automatic one-way drop links");
    }
  });
  test("overlapping floors remain separate", [] {
    Fixture f;
    f.floor(0, 0, 256, 256);
    f.floor(0, 0, 256, 256, 128);
    auto r = navmesh::build(f.map(), {});
    require(navmesh::reachable(*r.mesh, {64, 64, 1}, {192, 192, 1}),
            "lower floor lost");
    require(navmesh::reachable(*r.mesh, {64, 64, 129}, {192, 192, 129}),
            "upper floor lost");
    require(!navmesh::reachable(*r.mesh, {64, 64, 1}, {192, 192, 129}),
            "false floor connection");
  });
  test("negative world tile coordinates work", [] {
    Fixture f;
    f.floor(-2048, -2048, 0, 0);
    auto r = navmesh::build(f.map({-2048, -2048, -16}, {0, 0, 512}), {});
    require(r.tiles == 4 &&
                navmesh::reachable(*r.mesh, {-1920, -1920, 1}, {-128, -128, 1}),
            "negative world tiles lost");
  });
  test("cancellation stops tile building", [] {
    Fixture f;
    f.floor(0, 0, 2048, 2048);
    int done = 0;
    bool stopped = false;
    try {
      navmesh::build(
          f.map({0, 0, -16}, {2048, 2048, 512}), {}, [&] { return done == 1; },
          [&](auto d, auto) { done = static_cast<int>(d); });
    } catch (const navmesh::Cancelled &) {
      stopped = true;
    }
    require(stopped && done == 1, "cancellation must stop before second tile");
  });
  test("settings reject overflow and nonfinite values", [] {
    navmesh::validate({});
    auto s = navmesh::Settings{};
    s.cell_size = 0;
    rejects([&] { navmesh::validate(s); });
    s = {};
    s.actor_height = 256;
    rejects([&] { navmesh::validate(s); });
    s = {};
    s.tile_cells = 0;
    rejects([&] { navmesh::validate(s); });
    s = {};
    s.actor_radius = std::numeric_limits<float>::infinity();
    rejects([&] { navmesh::validate(s); });
  });
  test("unrepresentable height span fails instead of truncating", [] {
    Fixture f;
    f.floor(0, 0, 256, 256);
    f.floor(0, 0, 256, 256, 70000);
    rejects([&] { navmesh::build(f.map({0, 0, -16}, {256, 256, 70100}), {}); });
  });
  test("too many stacked layers are rejected before compact truncation", [] {
    Fixture f;
    for (int i = 0; i < 64; ++i)
      f.floor(0, 0, 256, 256, float(i * 128));
    rejects([&] { navmesh::build(f.map({0, 0, -16}, {256, 256, 9000}), {}); });
  });
  test("MSET round trip retains negative tiles and separate floors", [&] {
    Fixture f;
    f.floor(-2048, -2048, 0, 0);
    f.floor(-2048, -2048, 0, 0, 128);
    auto r = navmesh::build(f.map({-2048, -2048, -16}, {0, 0, 512}), {});
    Directory temp;
    const auto path = temp.path / "floors.navmesh";
    navmesh::save(*r.mesh, path);
    auto loaded = navmesh::load(path);
    require(navmesh::reachable(*loaded, {-1920, -1920, 1}, {-128, -128, 1}),
            "lost lower seam route");
    require(navmesh::reachable(*loaded, {-1920, -1920, 129}, {-128, -128, 129}),
            "lost upper seam route");
    require(!navmesh::reachable(*loaded, {-1920, -1920, 1}, {-128, -128, 129}),
            "layers joined on reload");
    rejects([&] { navmesh::save(*r.mesh, path); });
    require(navmesh::reachable(*navmesh::load(path), {-1920, -1920, 1},
                               {-128, -128, 1}),
            "overwrite damaged file");
    if (argc == 3 && std::string_view(argv[1]) == "--fixtures") {
      std::filesystem::create_directories(argv[2]);
      navmesh::save(*r.mesh, std::filesystem::path(argv[2]) / "floors.navmesh");
    }
  });
  test("MSET rejects truncated trailing and corrupt counts before Detour", [] {
    Fixture f;
    f.floor(0, 0, 256, 256);
    auto r = navmesh::build(f.map(), {});
    Directory temp;
    const auto path = temp.path / "mesh.navmesh",
               bad = temp.path / "bad.navmesh";
    navmesh::save(*r.mesh, path);
    std::ifstream in(path, std::ios::binary);
    const std::vector<char> bytes{std::istreambuf_iterator<char>(in), {}};
    auto check = [&](std::vector<char> data) {
      {
        std::ofstream out(bad, std::ios::binary);
        out.write(data.data(), data.size());
      }
      rejects([&] { navmesh::load(bad); });
    };
    auto data = bytes;
    data.pop_back();
    check(data);
    data = bytes;
    data.push_back(0);
    check(data);
    // MSET numTiles, maxTiles, tile byte length, then Detour poly count.
    for (std::size_t offset : {8U, 32U, 48U, 80U}) {
      data = bytes;
      for (int j = 0; j < 4; ++j)
        data[offset + j] = char(0xff);
      check(data);
    }
    // First polygon's first vertex index must remain within header.vertCount.
    unsigned verts = 0;
    for (int j = 0; j < 4; ++j)
      verts |= unsigned(static_cast<unsigned char>(bytes[84 + j])) << (j * 8);
    data = bytes;
    data[56 + 100 + verts * 12 + 4] = char(0xff);
    data[56 + 100 + verts * 12 + 5] = char(0xff);
    check(data);
  });
  test("failed publication leaves no final file or temporary artifacts", [] {
    Fixture f;
    f.floor(0, 0, 256, 256);
    auto r = navmesh::build(f.map(), {});
    Directory temp;
    // Saving validates the complete staged MSET before making it visible.
    // Deliberately invalidate the otherwise built mesh to exercise cleanup.
    const auto *tile =
        static_cast<const dtNavMesh &>(*r.mesh).getTileAt(0, 0, 0);
    require(tile && tile->header, "fixture tile exists");
    tile->header->version = 0;
    rejects([&] { navmesh::save(*r.mesh, temp.path / "invalid.navmesh"); });
    require(std::filesystem::is_empty(temp.path),
            "failed staged validation left output artifacts");
    rejects([&] {
      navmesh::save(*r.mesh, temp.path / "missing" / "output.navmesh");
    });
    require(!std::filesystem::exists(temp.path / "missing"),
            "save must not silently create output parents");
  });
  test("fractional cells at far world coordinates survive serialization", [] {
    for (float x : {164864.f, -164864.f}) {
      Fixture f;
      f.floor(x, 32768, x + 2048, 34816);
      auto settings = navmesh::Settings{};
      settings.cell_size = 16.1f;
      auto r = navmesh::build(f.map({x, 32768, -16}, {x + 2048, 34816, 512}),
                              settings);
      require(navmesh::reachable(*r.mesh, {x + 128, 32896, 1},
                                 {x + 1920, 34688, 1}),
              "fractional cell seam route lost before serialization");
      Directory temp;
      navmesh::save(*r.mesh, temp.path / "fractional.navmesh");
      const auto loaded = navmesh::load(temp.path / "fractional.navmesh");
      require(navmesh::reachable(*loaded, {x + 128, 32896, 1},
                                 {x + 1920, 34688, 1}),
              "fractional cell seam route lost");
    }
  });
  test("cancellation during staged save prevents final publication", [] {
    Fixture f;
    f.floor(0, 0, 256, 256);
    auto r = navmesh::build(f.map(), {});
    Directory temp;
    bool stopped = false;
    try {
      navmesh::save(*r.mesh, temp.path / "cancelled.navmesh", [&] {
        for (const auto &entry :
             std::filesystem::directory_iterator(temp.path)) {
          const auto staged = entry.path() / "mesh.part";
          if (std::filesystem::is_regular_file(staged) &&
              std::filesystem::file_size(staged) > 0)
            return true;
        }
        return false;
      });
    } catch (const navmesh::Cancelled &) {
      stopped = true;
    }
    require(stopped && std::filesystem::is_empty(temp.path),
            "cancel during save was ignored or left artifacts");
  });
  test("detail vertex accumulation cannot wrap pinned Detour ushort offsets",
       [] {
         // Each submesh stays below Recast's 127-vertex limit. The accumulated
         // extra detail vertices, not the base polygon vertices, overflow.
         constexpr unsigned count = 1024;
         std::vector<unsigned short> polys(count * 12, 0xffff);
         std::vector<unsigned int> detail(count * 4);
         for (unsigned i = 0; i < count; ++i) {
           polys[i * 12] = 0;
           polys[i * 12 + 1] = 1;
           polys[i * 12 + 2] = 2;
           detail[i * 4 + 1] = 67;
           detail[i * 4 + 3] = 120;
         }
         // 1024*64 - 1 = 65535 extra vertices: last valid accumulated offset.
         detail[1] = 66;
         navmesh::detail::validate_detail_limits(polys, 6, detail);
         detail[1] = 67;
         rejects([&] {
           navmesh::detail::validate_detail_limits(polys, 6, detail);
         });
       });
  std::cout << "Failures: " << failures << '\n';
  return failures ? 1 : 0;
}
