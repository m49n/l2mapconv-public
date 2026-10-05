// Offline acceptance helper: choose real connected surfaces, never add links.
#include <DetourNavMeshQuery.h>
#include <cmath>
#include <iostream>
#include <navmesh/WorldMesh.h>
#include <pathfinding/Case.h>
#include <queue>
#include <set>
#include <territory/PathIO.h>
#include <unordered_set>
int main(int argc, char **argv) {
  try {
    if (argc < 4)
      throw std::runtime_error("Usage: navmesh_world_probe NEW_DIRECTORY "
                               "mesh... (P542 acceptance set)");
    const auto output = std::filesystem::absolute(argv[1]);
    if (std::filesystem::exists(output))
      throw std::runtime_error("Probe output must be new");
    std::vector<navmesh::RegionFile> files;
    std::vector<pathfinding::NavRegionInput> inputs;
    for (int i = 2; i < argc; ++i) {
      const auto file = std::filesystem::canonical(argv[i]);
      const auto m = navmesh::read_region_metadata(file);
      files.push_back({m.map, file});
      inputs.push_back(
          {m.map, pathfinding::identify_file(file),
           pathfinding::identify_file(navmesh::metadata_path(file))});
    }
    auto world = navmesh::load_world(files);
    const auto &mesh = static_cast<const dtNavMesh &>(*world.mesh);
    dtNavMeshQuery query;
    if (dtStatusFailed(query.init(&mesh, 65535)))
      throw std::runtime_error("Query init");
    auto point = [&](dtPolyRef ref) {
      const dtMeshTile *tile = nullptr;
      const dtPoly *poly = nullptr;
      if (dtStatusFailed(mesh.getTileAndPolyByRef(ref, &tile, &poly)))
        throw std::runtime_error("Bad ref");
      float center[3]{};
      for (int v = 0; v < poly->vertCount; ++v)
        for (int k = 0; k < 3; ++k)
          center[k] += tile->verts[poly->verts[v] * 3 + k] / poly->vertCount;
      float closest[3];
      bool over = false;
      if (dtStatusFailed(
              query.closestPointOnPoly(ref, center, closest, &over)) ||
          !over)
        throw std::runtime_error("Poly centroid outside surface");
      return pathfinding::WorldPoint{closest[0], closest[2], closest[1]};
    };
    auto region = [&](dtPolyRef ref) {
      const dtMeshTile *tile = nullptr;
      const dtPoly *poly = nullptr;
      mesh.getTileAndPolyByRef(ref, &tile, &poly);
      for (const auto &r : world.regions) {
        const auto b = navmesh::region_bounds(r.map);
        if (tile->header->bmin[0] >= b.x && tile->header->bmin[0] < b.z &&
            tile->header->bmin[2] >= b.y && tile->header->bmin[2] < b.w)
          return r.map;
      }
      throw std::runtime_error("Unknown tile owner");
    };
    std::unordered_set<dtPolyRef> visited;
    std::vector<dtPolyRef> best;
    for (int i = 0; i < mesh.getMaxTiles(); ++i) {
      const auto *tile = mesh.getTile(i);
      if (!tile->header)
        continue;
      for (int j = 0; j < tile->header->polyCount; ++j) {
        const auto start = mesh.getPolyRefBase(tile) | dtPolyRef(j);
        if (visited.contains(start))
          continue;
        std::vector<dtPolyRef> component;
        std::queue<dtPolyRef> todo;
        todo.push(start);
        visited.insert(start);
        std::set<std::string> regions;
        while (!todo.empty()) {
          const auto ref = todo.front();
          todo.pop();
          component.push_back(ref);
          regions.insert(region(ref));
          const dtMeshTile *t = nullptr;
          const dtPoly *p = nullptr;
          mesh.getTileAndPolyByRef(ref, &t, &p);
          for (auto l = p->firstLink; l != DT_NULL_LINK; l = t->links[l].next) {
            const auto other = t->links[l].ref;
            if (other && visited.insert(other).second)
              todo.push(other);
          }
        }
        if (regions.size() == files.size() && component.size() > best.size())
          best = std::move(component);
      }
    }
    if (best.empty())
      throw std::runtime_error(
          "No connected component spans the requested set");
    std::map<std::string, dtPolyRef> selected;
    std::map<std::string, double> distance;
    std::map<std::pair<std::string, std::string>,
             std::pair<dtPolyRef, dtPolyRef>>
        seams;
    for (const auto ref : best) {
      const auto name = region(ref);
      const auto b = navmesh::region_bounds(name);
      const auto p = point(ref);
      const double d = std::hypot(p.x - (b.x + b.z) / 2, p.y - (b.y + b.w) / 2);
      if (!selected.contains(name) || d < distance[name]) {
        selected[name] = ref;
        distance[name] = d;
      }
      const dtMeshTile *tile = nullptr;
      const dtPoly *poly = nullptr;
      mesh.getTileAndPolyByRef(ref, &tile, &poly);
      for (auto l = poly->firstLink; l != DT_NULL_LINK;
           l = tile->links[l].next) {
        const auto other = tile->links[l].ref;
        if (!other)
          continue;
        const auto next = region(other);
        if (name < next && !seams.contains({name, next}))
          seams[{name, next}] = {ref, other};
      }
    }
    std::filesystem::create_directory(output);
    navmesh::save(mesh, output / "world.navmesh");
    auto make = [&](std::string id, dtPolyRef from, dtPolyRef to) {
      pathfinding::RouteCase c;
      c.id = id;
      c.map = region(from);
      c.nav_regions = inputs;
      c.a = {point(from), point(from).z};
      c.b = {point(to), point(to).z};
      pathfinding::write_case_new(c, output / (id + ".json"));
      return c;
    };
    auto summary = territory::Json::object();
    summary["component_polygons"] = best.size();
    summary["cases"] = territory::Json::array();
    for (const auto &[pair, refs] : seams) {
      const auto id = "seam-" + pair.first + "-" + pair.second;
      auto c = make(id, refs.first, refs.second);
      summary["cases"].push_back(id);
      std::swap(c.a, c.b);
      c.id = id + "-reverse";
      pathfinding::write_case_new(c, output / (c.id + ".json"));
      summary["cases"].push_back(c.id);
    }
    if (selected.contains("22_22") && selected.contains("23_21")) {
      auto c =
          make("three-forward", selected.at("22_22"), selected.at("23_21"));
      summary["cases"].push_back(c.id);
      std::swap(c.a, c.b);
      c.id = "three-reverse";
      pathfinding::write_case_new(c, output / (c.id + ".json"));
      summary["cases"].push_back(c.id);
      std::erase_if(c.nav_regions,
                    [](const auto &r) { return r.map == "22_21"; });
      c.id = "missing-middle";
      pathfinding::write_case_new(c, output / (c.id + ".json"));
      summary["cases"].push_back(c.id);
    }
    territory::write_json_atomic(output / "probe.json", summary, false);
    std::cout << summary.dump(2) << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
