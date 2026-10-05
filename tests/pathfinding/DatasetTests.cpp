#include "Support.h"
#include <array>
#include <cmath>
#include <pathfinding/Dataset.h>
#include "../navmesh/WorldFixture.h"
using namespace pathfinding;
using namespace pf_test;
namespace {
void packed(std::ostream &s, std::int16_t height, unsigned nswe) {
  const auto word = static_cast<std::uint16_t>(
      (static_cast<unsigned>(static_cast<std::uint16_t>(height)) << 1) | nswe);
  s.put(static_cast<char>(word & 255));
  s.put(static_cast<char>(word >> 8));
}
void fixture_l2j(const std::filesystem::path &p, std::int16_t flat_height = 0) {
  std::ofstream s(p, std::ios::binary);
  for (int bx = 0; bx < 256; ++bx)
    for (int by = 0; by < 256; ++by) {
      if (bx == 254 && by == 254) {
        s.put(2);
        for (int x = 0; x < 8; ++x)
          for (int y = 0; y < 8; ++y) {
            s.put(2);
            packed(s, 0, 15);
            packed(s, 128, (x == 0 && y == 0) ? 0 : 15);
          }
      } else if (bx == 255 && by == 255) {
        s.put(2);
        for (int i = 0; i < 64; ++i)
          s.put(0);
      } else {
        s.put(0);
        s.put(static_cast<char>(static_cast<std::uint16_t>(flat_height) & 255));
        s.put(static_cast<char>(static_cast<std::uint16_t>(flat_height) >> 8));
      }
    }
}
void fixture_nav(const std::filesystem::path &p) {
  auto mesh = std::make_shared<geodata::Mesh>();
  mesh->instance_matrices.push_back(glm::mat4{1});
  for (float z : {0.f, 128.f}) {
    const auto base = static_cast<unsigned>(mesh->vertices.size());
    for (auto xy :
         std::array<glm::vec2, 4>{{{-256, -256}, {0, -256}, {0, 0}, {-256, 0}}})
      mesh->vertices.push_back({{xy, z}, {0, 0, 1}});
    for (auto i : {0u, 1u, 2u, 0u, 2u, 3u})
      mesh->indices.push_back(base + i);
  }
  geodata::Map map{"fixture", geometry::Box{{-256, -256, -16}, {0, 0, 512}}};
  map.add({mesh, glm::mat4{1}});
  auto r = navmesh::build(map, {});
  navmesh::save(*r.mesh, p);
}
} // namespace
int dataset_tests() {
  TempDirectory temp;
  fixture_l2j(temp.path / "19_17.l2j");
  fixture_nav(temp.path / "19_17.navmesh");
  const auto l2j = identify_file(temp.path / "19_17.l2j"),
             nav = identify_file(temp.path / "19_17.navmesh");
  const auto ds = Dataset::load("19_17", l2j, nav);
  int n = 0;
  n += check("L-shaped dataset picks all regions but excludes missing quadrant",[&]{
    world_fixture::Directory d;world_fixture::Geometry g;
    g.floor(31744,31744,33792,33792);
    std::vector<NavRegionInput> inputs;
    for(const auto& map:{"20_18","20_19","21_18"}){
      const auto f=world_fixture::save(g,d.path,map);
      inputs.push_back({map,identify_file(f.mesh_path),identify_file(navmesh::metadata_path(f.mesh_path))});
    }
    const auto data=Dataset::load("20_19",l2j,inputs);
    const auto b=data.game_bounds();
    if(b!=glm::dvec4(0,0,65536,65536) || data.contains(33000,33000) || !data.candidates(33000,33000).empty())return false;
    for(auto p:{glm::dvec2{32512,32512},glm::dvec2{32512,33024},glm::dvec2{33024,32512}})
      if(data.candidates(p.x,p.y).empty() || !data.region_at(p.x,p.y))return false;
    const auto nav_only=data.candidates(33024,32512);
    if(std::any_of(nav_only.begin(),nav_only.end(),[](const auto& p){return p.surface_id.starts_with("l2j:");}))return false;
    bool west=false,north=false,east=false;
    for(const auto& poly:data.nav_polygons(-32,32))for(auto p:poly){west|=p.x<32768&&p.y<32768;north|=p.y>32768;east|=p.x>32768;}
    auto wrong=inputs;wrong[1].map="21_19";
    return west&&north&&east&&data.nav_regions().size()==3&&rejects([&]{Dataset::load("20_19",l2j,wrong);});
  });
  n += check("L2J flat floor picking matches the offline server's sixteen-unit "
             "height mask",
             [&] {
               fixture_l2j(temp.path / "flat-height.l2j", -3464);
               const auto data = Dataset::load(
                   "19_17", identify_file(temp.path / "flat-height.l2j"), FileIdentity{});
               const auto candidates = data.candidates(-32760, -32760);
               const auto cells = data.l2j_cells(
                   {-32768, -32768, -32641, -32641}, -3500, -3400, 64);
               return candidates.size() == 1 &&
                      candidates[0].resolved.z == -3472 && cells.size() == 1 &&
                      cells[0].minimum.z == -3472;
             });
  n += check(
      "each navigation format loads and picks without its counterpart", [&] {
        const auto geo = Dataset::load("19_17", l2j, FileIdentity{});
        const auto mesh = Dataset::load("19_17", {}, nav);
        const auto gc = geo.candidates(-200, -200),
                   nc = mesh.candidates(-200, -200);
        return gc.size() == 2 && nc.size() == 2 &&
               geo.nav_polygons(-1000, 1000).empty() &&
               mesh.l2j_overview({-32768, -32768, 0, 0}, -1000, 1000, 128)
                   .empty() &&
               std::all_of(
                   gc.begin(), gc.end(),
                   [](auto &v) { return v.surface_id.starts_with("l2j:"); }) &&
               std::all_of(nc.begin(), nc.end(), [](auto &v) {
                 return v.surface_id.starts_with("nav:");
               });
      });
  n += check("dataset refuses an empty navigation selection",
             [&] { return rejects([&] { Dataset::load("19_17", {}, FileIdentity{}); }); });
  n += check("overlapping layers keep actual L2J and nav surface IDs", [&] {
    auto c = ds.candidates(-200, -200);
    bool low = false, high = false, nlow = false, nhigh = false;
    for (auto &p : c) {
      if (p.surface_id.starts_with("l2j:")) {
        low |= p.resolved.z == 0;
        high |= p.resolved.z == 128;
      }
      if (p.surface_id.starts_with("nav:")) {
        nlow |= std::abs(p.resolved.z - 1) < 0.01;
        nhigh |= std::abs(p.resolved.z - 129) < 0.01;
      }
    }
    return low && high && nlow && nhigh;
  });
  n += check("zero NSWE layer remains visible but labelled unwalkable", [&] {
    auto c = ds.candidates(-248, -248);
    return std::any_of(c.begin(), c.end(), [](auto &p) {
      return p.surface_id.starts_with("l2j:") && p.resolved.z == 128 &&
             !p.valid;
    });
  });
  n += check("simple L2J block owns all 64 columns without expansion", [&] {
    auto c = ds.candidates(-32760, -32760);
    return c.size() == 1 && c.front().resolved.z == 0;
  });
  n += check("empty columns and map edge never invent floor", [&] {
    return ds.candidates(-1, -1).empty() && ds.candidates(0, 0).empty() &&
           ds.contains(-32768, -32768) && !ds.contains(0, 0);
  });
  n += check("visible L2J overlay bounded and filtered by actual layer", [&] {
    auto cells = ds.l2j_cells({-256, -256, 0, 0}, 100, 140, 5);
    return cells.size() == 5 &&
           std::all_of(cells.begin(), cells.end(), [](auto &c) {
             return c.minimum.z == 128 && c.width == 16;
           });
  });
  n += check("nav polygon slice includes actual intersecting surfaces", [&] {
    return !ds.nav_polygons(0, 4).empty() && ds.nav_polygons(32, 64).empty();
  });
  n += check(
      "L2J overview covers both ends of a whole region without prefix "
      "truncation",
      [&] {
        const auto cells = ds.l2j_overview({-32768, -32768, 0, 0}, -1, 1, 128);
        return cells.size() == 65535 && cells.front().minimum.x == -32768 &&
               std::any_of(cells.begin(), cells.end(), [](const auto &c) {
                 return c.minimum.x == -128 && c.minimum.y == -256;
               });
      });
  n +=
      check("L2J close view expands simple blocks and retains exact "
            "directional masks",
            [&] {
              const auto cells =
                  ds.l2j_overview({-32768, -32768, -32640, -32640}, -1, 1, 16);
              return cells.size() == 64 &&
                     std::all_of(cells.begin(), cells.end(), [](const auto &c) {
                       return c.detailed && c.nswe == 15 && c.width == 16;
                     });
            });
  n += check(
      "overview distinguishes mixed layers from exact directional cells", [&] {
        const auto both =
            ds.l2j_overview({-256, -256, -240, -240}, -1, 140, 16);
        const auto high =
            ds.l2j_overview({-256, -256, -240, -240}, 100, 140, 16);
        const auto coarse =
            ds.l2j_overview({-256, -256, -128, -128}, 100, 140, 128);
        return both.size() == 1 && both[0].mixed && !both[0].detailed &&
               high.size() == 1 && high[0].detailed && high[0].nswe == 0 &&
               coarse.size() == 1 && coarse[0].mixed && !coarse[0].detailed;
      });
  n += check(
      "overview respects empty slices and half-open outside-map bounds", [&] {
        return ds.l2j_overview({-256, -256, 0, 0}, 32, 64, 128).empty() &&
               ds.l2j_overview({0, 0, 128, 128}, -1, 140, 128).empty() &&
               rejects([&] { ds.l2j_overview({-256, -256, 0, 0}, 0, 1, 17); });
      });
  n += check("truncated L2J rejected before unsafe legacy loader", [&] {
    temp.file("broken.l2j", "\x02");
    return rejects([&] {
      Dataset::load("19_17", identify_file(temp.path / "broken.l2j"), nav);
    });
  });
  n += check("dataset loading refuses changed identities", [&] {
    auto wrong = l2j;
    wrong.size++;
    return rejects([&] { Dataset::load("19_17", wrong, nav); });
  });
  return n;
}

void write_route_fixtures(const std::filesystem::path &directory) {
  if (!std::filesystem::create_directory(directory))
    throw std::runtime_error("Fixture directory must be new");
  for (const std::string kind : {"wall", "stairs", "blocked"}) {
    auto mesh = std::make_shared<geodata::Mesh>();
    mesh->instance_matrices.push_back(glm::mat4{1});
    auto quad = [&](std::array<glm::vec3, 4> vertices, glm::vec3 normal) {
      auto base = static_cast<unsigned>(mesh->vertices.size());
      for (auto p : vertices)
        mesh->vertices.push_back({p, normal});
      for (auto i : {0u, 1u, 2u, 0u, 2u, 3u})
        mesh->indices.push_back(base + i);
    };
    auto floor = [&](float x0, float x1, float z) {
      quad({{{x0, 0, z}, {x1, 0, z}, {x1, 256, z}, {x0, 256, z}}}, {0, 0, 1});
    };
    if (kind == "stairs") {
      floor(0, 96, 0);
      floor(96, 128, 8);
      floor(128, 160, 16);
      floor(160, 256, 24);
    } else {
      floor(0, 256, 0);
      const float end = kind == "wall" ? 144 : 256;
      quad({{{128, 0, -16}, {128, end, -16}, {128, end, 192}, {128, 0, 192}}},
           {1, 0, 0});
    }
    geodata::Map map{kind, geometry::Box{{0, 0, -16}, {256, 256, 512}}};
    map.add({mesh, glm::mat4{1}});
    auto result = navmesh::build(map, {});
    navmesh::save(*result.mesh, directory / (kind + ".navmesh"));
    if (!navmesh::reachable(*result.mesh, {64, 96, 1},
                            {192, 96, kind == "stairs" ? 25.f : 1.f}) &&
        kind != "blocked")
      throw std::runtime_error("Native route fixture unexpectedly unreachable");
    if (kind == "blocked" &&
        navmesh::reachable(*result.mesh, {64, 96, 1}, {192, 96, 1}))
      throw std::runtime_error("Native blocked fixture unexpectedly reachable");
  }
}
