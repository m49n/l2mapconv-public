#include <algorithm>
#include <charconv>
#include <cmath>
#include <pathfinding/Case.h>
#include <stdexcept>
#include <territory/FileIdentity.h>
#include <territory/PathIO.h>
#include <navmesh/RegionMetadata.h>
#include <set>
namespace pathfinding {
std::vector<NavRegionInput> navmesh_region_inputs(const RouteCase& c) {
  if(!c.nav_regions.empty()) return c.nav_regions;
  if(has_input(c.navmesh)) return {{c.map,c.navmesh,{}}};
  return {};
}
std::vector<std::string> navigation_regions(const RouteCase& c) {
  std::set<std::string> regions;
  if(has_input(c.l2j))regions.insert(c.map);
  for(const auto& r:navmesh_region_inputs(c))regions.insert(r.map);
  return {regions.begin(),regions.end()};
}
bool backend_scope_supported(const RouteCase& c,const std::string& backend) {
  std::vector<std::string> maps;
  if(backend=="l2j" && has_input(c.l2j)) maps.push_back(c.map);
  if(backend=="navmesh")for(const auto& r:navmesh_region_inputs(c))maps.push_back(r.map);
  for(const auto& e:{c.a,c.b}) {
    bool found=false;
    for(const auto& name:maps) {
      const auto o=region_origin(name);
      found |= e.requested.x>=o.x && e.requested.x<o.x+32768 && e.requested.y>=o.y && e.requested.y<o.y+32768;
    }
    if(!found)return false;
  }
  return true;
}
bool has_input(const FileIdentity &f) { return !f.path.empty(); }
std::vector<std::string> enabled_backends(const RouteCase &c) {
  std::vector<std::string> out;
  if (has_input(c.l2j))
    out.emplace_back("l2j");
  if (has_input(c.navmesh) || !c.nav_regions.empty())
    out.emplace_back("navmesh");
  return out;
}
void verify_backend_identity(const RouteCase &c, const std::string &backend,
                             const std::string &actual_sha256) {
  const auto expected = c.expected_backends.find(backend);
  if (expected != c.expected_backends.end() &&
      expected->second != actual_sha256)
    throw std::invalid_argument(
        "Backend identity changed: " + backend +
        "; explicitly accept the new backend for a new comparison");
}
namespace {
void require(bool condition, const char *message) {
  if (!condition)
    throw std::invalid_argument(message);
}
void valid_coordinate(double value) {
  require(std::isfinite(value) && std::abs(value) <= 10000000,
          "Coordinate must be finite and within world limits");
}
void validate_sha256(const std::string &sha256) {
  require(sha256.size() == 64 && std::all_of(sha256.begin(), sha256.end(),
                                             [](char c) {
                                               return (c >= '0' && c <= '9') ||
                                                      (c >= 'a' && c <= 'f');
                                             }),
          "Invalid SHA256");
}
void validate_identity(const FileIdentity &f) {
  require(!f.path.empty() && f.path.is_absolute(),
          "Input path must be absolute");
  validate_sha256(f.sha256);
}
std::filesystem::path profile_path(const Json &j) {
  const auto p = territory::path_from_utf8(j.get<std::string>());
  require(p.empty() || p.is_absolute(),
          "Backend paths must be absolute or explicitly unavailable");
  return p;
}
Endpoint endpoint_from_json(const Json &j) {
  require(j.at("confirmed_z").is_number(), "Invalid confirmed height");
  Endpoint e{point_from_json(j.at("requested")),
             j.at("confirmed_z").get<double>()};
  if (j.contains("backend_z"))
    e.backend_z = j.at("backend_z").get<std::map<std::string, double>>();
  return e;
}
Json endpoint_json(const Endpoint &p) {
  valid_coordinate(p.confirmed_z);
  Json j{{"requested", point_json(p.requested)},
         {"confirmed_z", p.confirmed_z}};
  if (!p.backend_z.empty())
    j["backend_z"] = p.backend_z;
  return j;
}
} // namespace
void validate_search_settings(const SearchSettings &s) {
  require(s.server_budget_ms >= 1 && s.server_budget_ms <= 30000,
          "Server reference budget must be 1..30000 ms");
  require(s.timeout_ms >= 1 && s.timeout_ms <= 30000,
          "Experimental timeout must be 1..30000 ms");
  require(s.nav_max_nodes >= 1 && s.nav_max_nodes <= 1000000,
          "Navmesh expansion limit must be 1..1000000");
  require(s.l2j_max_window_cells >= 64 && s.l2j_max_window_cells <= 2048 &&
              s.l2j_max_window_cells % 32 == 0,
          "L2J window must be 64..2048 cells in steps of 32");
}
Json search_settings_json(const SearchSettings &s) {
  validate_search_settings(s);
  return {{"server_budget_ms", s.server_budget_ms},
          {"timeout_ms", s.timeout_ms},
          {"nav_max_nodes", s.nav_max_nodes},
          {"l2j_max_window_cells", s.l2j_max_window_cells}};
}
void validate_point(WorldPoint p) {
  valid_coordinate(p.x);
  valid_coordinate(p.y);
  valid_coordinate(p.z);
}
WorldPoint point_from_json(const Json &j) {
  require(j.is_array() && j.size() == 3, "Point must contain XYZ");
  for (const auto &v : j)
    require(v.is_number(), "Point component must be numeric");
  WorldPoint p{j[0].get<double>(), j[1].get<double>(), j[2].get<double>()};
  validate_point(p);
  return p;
}
Json point_json(WorldPoint p) {
  validate_point(p);
  return Json::array({p.x, p.y, p.z});
}
WorldPoint region_origin(const std::string &map) {
  require(map.size() == 5 && map[2] == '_', "Map must be XX_YY");
  int x{}, y{};
  auto a = std::from_chars(map.data(), map.data() + 2, x),
       b = std::from_chars(map.data() + 3, map.data() + 5, y);
  require(a.ec == std::errc{} && a.ptr == map.data() + 2 &&
              b.ec == std::errc{} && b.ptr == map.data() + 5 && x >= 0 &&
              y >= 0,
          "Invalid map coordinates");
  return {(x - 20) * 32768.0, (y - 18) * 32768.0, 0};
}
void validate_case(const RouteCase &c) {
  require(!c.id.empty() && c.id.size() <= 128 &&
              std::all_of(c.id.begin(), c.id.end(),
                          [](char ch) {
                            return (ch >= 'a' && ch <= 'z') ||
                                   (ch >= 'A' && ch <= 'Z') ||
                                   (ch >= '0' && ch <= '9') || ch == '_' ||
                                   ch == '-' || ch == '.';
                          }),
          "Case ID must be a short plain identifier");
  region_origin(c.map);
  require(c.nav_regions.size()<=1024,"Too many navigation regions");
  require(c.nav_regions.empty() || !has_input(c.navmesh),"Cannot combine legacy navmesh and region list");
  std::set<std::string> maps;
  for(const auto& r:c.nav_regions) {
    region_origin(r.map);
    require(maps.insert(r.map).second,"Duplicate navigation region");
    validate_identity(r.mesh);validate_identity(r.metadata);
    require(r.metadata.path==navmesh::metadata_path(r.mesh.path),"Navmesh metadata must accompany its mesh");
  }
  const auto regions=navigation_regions(c);
  for (const auto &e : {c.a, c.b}) {
    validate_point(e.requested);
    valid_coordinate(e.confirmed_z);
    for (const auto &[backend, z] : e.backend_z) {
      require(backend == "l2j" || backend == "navmesh",
              "Unknown endpoint backend");
      valid_coordinate(z);
      require(std::abs(z - e.confirmed_z) <= 32,
              "Backend floor exceeds selected floor tolerance");
    }
    bool contained=false;
    for(const auto& name:regions) {
      const auto origin=region_origin(name);
      contained |= e.requested.x>=origin.x && e.requested.x<origin.x+32768 && e.requested.y>=origin.y && e.requested.y<origin.y+32768;
    }
    require(contained,"Endpoint is outside loaded navigation regions");
  }
  require(std::isfinite(c.snap_horizontal) && c.snap_horizontal > 0 &&
              c.snap_horizontal <= 32768 && std::isfinite(c.snap_vertical) &&
              c.snap_vertical > 0 && c.snap_vertical <= 32768,
          "Invalid snap limits");
  require(has_input(c.l2j) || has_input(c.navmesh) || !c.nav_regions.empty(),
          "Select at least one navigation format");
  for (const auto &f : {c.l2j, c.navmesh}) {
    if (has_input(f))
      validate_identity(f);
    else
      require(f.sha256.empty() && f.size == 0,
              "Incomplete navigation identity");
  }
  validate_search_settings(c.search);
  for (const auto &[backend, hash] : c.expected_backends) {
    require(backend == "l2j" || backend == "navmesh",
            "Unknown backend baseline");
    validate_sha256(hash);
  }
}
RouteCase case_for_backend(const RouteCase &source,
                           const std::string &backend) {
  validate_case(source);
  auto c = source;
  for (auto *e : {&c.a, &c.b}) {
    if (const auto z = e->backend_z.find(backend); z != e->backend_z.end())
      e->requested.z = e->confirmed_z = z->second;
    e->backend_z.clear();
  }
  return c;
}
Json identity_json(const FileIdentity &f) {
  validate_identity(f);
  return {{"path", territory::path_utf8(f.path)},
          {"sha256", f.sha256},
          {"size", f.size}};
}
FileIdentity identity_from_json(const Json &j) {
  require(j.at("size").is_number_unsigned() ||
              (j.at("size").is_number_integer() &&
               j.at("size").get<std::int64_t>() >= 0),
          "Invalid file size");
  FileIdentity f{territory::path_from_utf8(j.at("path").get<std::string>()),
                 j.at("sha256").get<std::string>(),
                 j.at("size").get<std::uint64_t>()};
  std::transform(
      f.sha256.begin(), f.sha256.end(), f.sha256.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  validate_identity(f);
  return f;
}
FileIdentity identify_file(const std::filesystem::path &p) {
  const auto f = territory::file_identity(std::filesystem::canonical(p));
  return {f.path, f.sha256, f.bytes};
}
void verify_identity(const FileIdentity &f) {
  validate_identity(f);
  const auto current = identify_file(f.path);
  if (current.sha256 != f.sha256 || current.size != f.size)
    throw std::runtime_error("Input identity changed: " +
                             territory::path_utf8(f.path));
}
Json case_json(const RouteCase &c) {
  validate_case(c);
  Json j{{"schema", 1},
         {"case_id", c.id},
         {"map", c.map},
         {"a", endpoint_json(c.a)},
         {"b", endpoint_json(c.b)},
         {"l2j", has_input(c.l2j) ? identity_json(c.l2j) : Json(nullptr)},
         {"navmesh",
          has_input(c.navmesh) ? identity_json(c.navmesh) : Json(nullptr)},
         {"search_settings", search_settings_json(c.search)},
         {"snap_horizontal", c.snap_horizontal},
         {"snap_vertical", c.snap_vertical}};
  if (!c.expected_backends.empty())
    j["expected_backends"] = c.expected_backends;
  if(!c.nav_regions.empty()) {
    j["schema"]=2;j.erase("navmesh");j["nav_regions"]=Json::array();
    for(const auto& r:c.nav_regions)
      j["nav_regions"].push_back({{"map",r.map},{"mesh",identity_json(r.mesh)},{"metadata",identity_json(r.metadata)}});
  }
  return j;
}
RouteCase case_from_json(const Json &j) {
  require(j.at("schema").is_number_integer() && (j.at("schema") == 1 || j.at("schema")==2),
          "Unsupported case schema");
  RouteCase c{j.at("case_id").get<std::string>(),
              j.at("map").get<std::string>(),
              endpoint_from_json(j.at("a")),
              endpoint_from_json(j.at("b")),
              ((j.at("schema")==2 && !j.contains("l2j")) || j.at("l2j").is_null()) ? FileIdentity{}
                                    : identity_from_json(j.at("l2j")),
              !j.contains("navmesh") || j.at("navmesh").is_null() ? FileIdentity{}
                                        : identity_from_json(j.at("navmesh")),
              j.at("snap_horizontal").get<double>(),
              j.at("snap_vertical").get<double>()};
  if(j.at("schema")==2) {
    const auto& regions=j.at("nav_regions");
    require(regions.is_array() && !regions.empty() && regions.size()<=1024,"Invalid navigation region list");
    for(const auto& r:regions)c.nav_regions.push_back({r.at("map").get<std::string>(),identity_from_json(r.at("mesh")),identity_from_json(r.at("metadata"))});
  } else require(!j.contains("nav_regions"),"Region list requires case schema2");
  if (j.contains("search_settings")) {
    const auto &s = j.at("search_settings");
    for (const char *key : {"server_budget_ms", "timeout_ms", "nav_max_nodes",
                            "l2j_max_window_cells"})
      require(s.at(key).is_number_integer() && s.at(key).get<double>() >= 0 &&
                  s.at(key).get<double>() <= 1000000,
              "Invalid integer search setting");
    c.search = {s.at("server_budget_ms").get<int>(),
                s.at("timeout_ms").get<int>(), s.at("nav_max_nodes").get<int>(),
                s.at("l2j_max_window_cells").get<int>()};
  }
  if (j.contains("expected_backends")) {
    require(j.at("expected_backends").is_object() &&
                j.at("expected_backends").size() <= 2,
            "Invalid backend baseline");
    c.expected_backends =
        j.at("expected_backends").get<std::map<std::string, std::string>>();
  }
  validate_case(c);
  return c;
}
RouteCase read_case(const std::filesystem::path &p) {
  return case_from_json(territory::read_json(p));
}
void write_case_new(const RouteCase &c, const std::filesystem::path &p) {
  territory::write_json_atomic(p, case_json(c), false);
}
Json profile_json(const BackendProfile &p) {
  Json cp = Json::array();
  for (const auto &path : p.legacy_classpath)
    cp.push_back(territory::path_utf8(path));
  Json j{{"schema", 1},
         {"java", territory::path_utf8(p.java)},
         {"legacy_classpath", cp},
         {"legacy_config", territory::path_utf8(p.legacy_config)},
         {"nav_runner_directory", territory::path_utf8(p.nav_runner_directory)},
         {"detour_jar", territory::path_utf8(p.detour_jar)},
         {"legacy_experimental_limits", p.legacy_experimental_limits}};
  (void)profile_from_json(j);
  return j;
}
BackendProfile profile_from_json(const Json &j) {
  require(j.at("schema").is_number_integer() && j.at("schema") == 1,
          "Unsupported backend profile schema");
  BackendProfile p;
  p.java = profile_path(j.at("java"));
  const auto &cp = j.at("legacy_classpath");
  require(cp.is_array() && cp.size() <= 1024, "Invalid classpath");
  for (const auto &v : cp) {
    auto path = profile_path(v);
    require(!path.empty(), "Empty classpath entry");
    p.legacy_classpath.push_back(path);
  }
  p.legacy_config = profile_path(j.at("legacy_config"));
  p.nav_runner_directory = profile_path(j.at("nav_runner_directory"));
  p.detour_jar = profile_path(j.at("detour_jar"));
  if (j.contains("legacy_experimental_limits")) {
    require(j.at("legacy_experimental_limits").is_boolean(),
            "Invalid legacy capability flag");
    p.legacy_experimental_limits =
        j.at("legacy_experimental_limits").get<bool>();
  }
  return p;
}
BackendProfile read_profile(const std::filesystem::path &file) {
  auto json = territory::read_json(file);
  const auto directory = std::filesystem::absolute(file).parent_path();
  const auto resolve = [&](Json &value) {
    auto path = territory::path_from_utf8(value.get<std::string>());
    if (!path.empty() && !path.is_absolute()) {
      require(!path.has_root_path(), "Ambiguous rooted backend path");
      path = (directory / path).lexically_normal();
      value = territory::path_utf8(path);
    }
  };
  for (const auto *name :
       {"java", "legacy_config", "nav_runner_directory", "detour_jar"})
    resolve(json.at(name));
  require(json.at("legacy_classpath").is_array(), "Invalid classpath");
  for (auto &entry : json.at("legacy_classpath"))
    resolve(entry);
  return profile_from_json(json);
}
std::filesystem::path
default_backend_profile_path(const std::filesystem::path &executable_directory,
                             const std::filesystem::path &selected_override) {
  if (!selected_override.empty())
    return selected_override;
  const auto bundled = std::filesystem::absolute(executable_directory) /
                       "pathfinding-backend/profile.json";
  return std::filesystem::is_regular_file(bundled) ? bundled
                                                   : std::filesystem::path{};
}
std::vector<FileIdentity> input_changes(const RouteCase &c) {
  std::vector<FileIdentity> changed;
  std::vector<FileIdentity> inputs{c.l2j,c.navmesh};
  for(const auto& r:c.nav_regions){inputs.push_back(r.mesh);inputs.push_back(r.metadata);}
  for (const auto &f : inputs) {
    if (!has_input(f))
      continue;
    try {
      verify_identity(f);
    } catch (const std::exception &) {
      changed.push_back(f);
    }
  }
  return changed;
}
RouteCase with_current_identities(const RouteCase &c) {
  auto result = c;
  if (has_input(c.l2j))
    result.l2j = identify_file(c.l2j.path);
  if (has_input(c.navmesh))
    result.navmesh = identify_file(c.navmesh.path);
  for(auto& r:result.nav_regions){r.mesh=identify_file(r.mesh.path);r.metadata=identify_file(r.metadata.path);}
  return result;
}
} // namespace pathfinding
