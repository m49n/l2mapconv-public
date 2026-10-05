#include "navmesh/WorldFixture.h"
#include <DetourAlloc.h>
#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <territory/PathIO.h>
namespace {
struct alignas(std::max_align_t) Allocation {
  std::size_t size;
};
std::size_t live_bytes = 0, peak_bytes = 0;
void *measured_alloc(std::size_t size, dtAllocHint) {
  auto *p = static_cast<Allocation *>(std::malloc(sizeof(Allocation) + size));
  if (!p)
    return nullptr;
  p->size = size;
  live_bytes += size;
  peak_bytes = std::max(peak_bytes, live_bytes);
  return p + 1;
}
void measured_free(void *value) {
  if (value) {
    auto *p = static_cast<Allocation *>(value) - 1;
    live_bytes -= p->size;
    std::free(p);
  }
}
struct MeasureAllocations {
  MeasureAllocations() {
    live_bytes = peak_bytes = 0;
    dtAllocSetCustom(measured_alloc, measured_free);
  }
  ~MeasureAllocations() { dtAllocSetCustom(nullptr, nullptr); }
};
void check(bool ok, const char *text) {
  if (!ok)
    throw std::runtime_error(text);
}
void rewrite_fixture(const std::filesystem::path &path,
                     const territory::Json &json) {
  std::ofstream(path) << json.dump();
}
template <class F> void rejects(F f) {
  bool rejected = false;
  try {
    f();
  } catch (const std::exception &) {
    rejected = true;
  }
  check(rejected, "invalid world accepted");
}
} // namespace
int main(int argc, char **argv) {
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
  using namespace world_fixture;
  test("sparse region capacity is not retained across inputs", [] {
    Directory d;
    Geometry g;
    g.floor(32000, 0, 33536, 1024);
    std::vector files{save(g, d.path, "20_18"), save(g, d.path, "21_18")};
    for (const auto &f : files) {
      {
        std::fstream stream(f.mesh_path,
                            std::ios::binary | std::ios::in | std::ios::out);
        const std::int32_t capacity = 65536;
        stream.seekp(32);
        stream.write(reinterpret_cast<const char *>(&capacity), 4);
      }
      auto metadata = territory::read_json(navmesh::metadata_path(f.mesh_path));
      metadata["mesh"]["sha256"] = territory::file_identity(f.mesh_path).sha256;
      rewrite_fixture(navmesh::metadata_path(f.mesh_path), metadata);
    }
    MeasureAllocations measurement;
    {
      auto world = navmesh::load_world(files);
      check(navmesh::reachable(*world.mesh, {32512, 256, 1}, {33024, 256, 1}),
            "sparse seam lost");
    }
    check(live_bytes == 0, "Detour allocation leak");
    check(peak_bytes < 10 * 1024 * 1024,
          "multiple empty Detour capacity tables retained");
  });
  test("world floor crosses regional seam both ways and reassigns refs", [&] {
    Directory d;
    Geometry g;
    g.floor(32000, 0, 33536, 1024);
    std::vector files{save(g, d.path, "20_18"), save(g, d.path, "21_18")};
    check(!navmesh::reachable(*navmesh::load(files[0].mesh_path),
                              {32512, 256, 1}, {33024, 256, 1}),
          "isolated baseline unexpectedly crosses");
    auto world = navmesh::load_world(files);
    check(navmesh::reachable(*world.mesh, {32512, 256, 1}, {33024, 256, 1}),
          "regional seam disconnected");
    check(navmesh::reachable(*world.mesh, {33024, 256, 1}, {32512, 256, 1}),
          "reverse seam disconnected");
    navmesh::save(*world.mesh, d.path / "merged.navmesh");
    check(navmesh::reachable(*navmesh::load(d.path / "merged.navmesh"),
                             {32512, 256, 1}, {33024, 256, 1}),
          "saved merge disconnected");
    std::reverse(files.begin(), files.end());
    check(navmesh::reachable(*navmesh::load_world(files).mesh, {32512, 256, 1},
                             {33024, 256, 1}),
          "load order changed reachability");
    if (argc == 3 && std::string_view(argv[1]) == "--fixtures") {
      std::filesystem::create_directories(argv[2]);
      navmesh::save(*world.mesh,
                    std::filesystem::path(argv[2]) / "world.navmesh");
    }
  });
  test("stacked seam floors stay separate", [&] {
    Directory d;
    Geometry g;
    g.floor(32000, 0, 33536, 1024);
    g.floor(32000, 0, 33536, 1024, 128);
    std::vector files{save(g, d.path, "20_18"), save(g, d.path, "21_18")};
    auto world = navmesh::load_world(files);
    for (float z : {1.f, 129.f})
      check(navmesh::reachable(*world.mesh, {32512, 256, z}, {33024, 256, z}),
            "floor lost across seam");
    check(!navmesh::reachable(*world.mesh, {32512, 256, 1}, {33024, 256, 129}),
          "floors cross-linked");
    if (argc == 3 && std::string_view(argv[1]) == "--fixtures")
      navmesh::save(*world.mesh,
                    std::filesystem::path(argv[2]) / "world-floors.navmesh");
  });
  test("wall high ledge and small step", [&] {
    for (int kind : {0, 1, 2}) {
      Directory d;
      Geometry g;
      g.floor(32000, 0, 32768, 1024);
      const float h = kind == 1 ? 64 : kind == 2 ? 8 : 0;
      g.floor(32768, 0, 33536, 1024, h);
      if (kind == 0)
        g.wall(32768, 0, 1024);
      std::vector files{save(g, d.path, "20_18"), save(g, d.path, "21_18")};
      auto world = navmesh::load_world(files);
      check(navmesh::reachable(*world.mesh, {32512, 256, 1},
                               {33024, 256, h + 1}) == (kind == 2),
            "wrong obstacle/climb seam");
      check(navmesh::reachable(*world.mesh, {33024, 256, h + 1},
                               {32512, 256, 1}) == (kind == 2),
            "wrong reverse obstacle/climb seam");
      if (kind == 0 && argc == 3 && std::string_view(argv[1]) == "--fixtures")
        navmesh::save(*world.mesh,
                      std::filesystem::path(argv[2]) / "world-wall.navmesh");
    }
  });
  test("four corners negative coordinates and missing middle", [] {
    Directory d;
    Geometry g;
    g.floor(-1024, -1024, 1024, 1024);
    std::vector files{save(g, d.path, "19_17"), save(g, d.path, "20_17"),
                      save(g, d.path, "19_18"), save(g, d.path, "20_18")};
    auto world = navmesh::load_world(files);
    check(navmesh::reachable(*world.mesh, {-256, -256, 1}, {256, 256, 1}),
          "four-corner world disconnected");
    files.erase(files.begin() + 1, files.begin() + 3);
    check(!navmesh::reachable(*navmesh::load_world(files).mesh, {-256, -256, 1},
                              {256, 256, 1}),
          "corner-only false connection");
  });
  test("invalid ownership metadata profile and sources rejected", [] {
    Directory d;
    Geometry g;
    g.floor(32000, 0, 33536, 1024);
    std::vector files{save(g, d.path, "20_18"), save(g, d.path, "21_18")};
    auto duplicate = files;
    duplicate.push_back(files[0]);
    rejects([&] { navmesh::load_world(duplicate); });
    auto wrong = files;
    wrong[1].map = "22_18";
    rejects([&] { navmesh::load_world(wrong); });
    const auto sidecar = navmesh::metadata_path(files[1].mesh_path);
    const auto original = territory::read_json(sidecar);
    auto altered = original;
    altered["profile"]["max_slope"] = 40;
    rewrite_fixture(sidecar, altered);
    rejects([&] { navmesh::load_world(files); });
    altered = original;
    altered["sources"][0]["sha256"] = std::string(64, 'b');
    rewrite_fixture(sidecar, altered);
    rejects([&] { navmesh::load_world(files); });
    altered = original;
    auto &loaded = altered["loaded_neighbors"];
    loaded.erase(std::find(loaded.begin(), loaded.end(), "20_18"));
    altered["missing_neighbors"].push_back("20_18");
    rewrite_fixture(sidecar, altered);
    rejects([&] { navmesh::load_world(files); });
    rewrite_fixture(sidecar, original);
    rejects([&] { navmesh::load_world(files, [] { return true; }); });
  });
  return failures ? 1 : 0;
}
