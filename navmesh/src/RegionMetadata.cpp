#include <algorithm>
#include <cmath>
#include <navmesh/RegionMetadata.h>
#include <set>
#include <stdexcept>
#include <territory/PathIO.h>
namespace navmesh {
namespace {
void require(bool ok, const char *text) {
  if (!ok)
    throw std::runtime_error(text);
}
bool hash(const std::string &s) {
  return s.size() == 64 &&
         s.find_first_not_of("0123456789abcdef") == std::string::npos;
}
void validate_metadata(const RegionMetadata &m) {
  region_bounds(m.map);
  validate_region_grid(m.settings);
  require(m.neighbor_context && m.padding == context_padding(m.settings),
          "Navmesh sidecar lacks correct neighbor context");
  require(!m.generator.empty() && m.generator.size() < 1024,
          "Invalid navmesh generator identity");
  auto neighbors = region_neighbors(m.map);
  std::set<std::string> recorded;
  for (const auto &list : {m.loaded_neighbors, m.missing_neighbors})
    for (const auto &name : list)
      require(std::find(neighbors.begin(), neighbors.end(), name) !=
                      neighbors.end() &&
                  recorded.insert(name).second,
              "Invalid or duplicate navmesh neighbor");
  require(recorded.size() == neighbors.size(),
          "Incomplete navmesh neighbor manifest");
  require(hash(m.mesh.sha256) && m.mesh.bytes > 0 &&
              m.mesh.bytes <= 512ULL * 1024 * 1024,
          "Invalid navmesh mesh identity");
  require(!m.sources.empty() && m.sources.size() <= 100000,
          "Invalid navmesh package manifest size");
  std::set<std::string> paths;
  for (const auto &source : m.sources) {
    const auto path = territory::path_from_utf8(source.relative_path);
    require(!path.empty() && !path.is_absolute() && !path.has_root_name() &&
                source.relative_path.find('\\') == std::string::npos &&
                path.generic_string() ==
                    path.lexically_normal().generic_string(),
            "Invalid navmesh source path");
    for (const auto &part : path)
      require(part != ".." && part != ".", "Escaping navmesh source path");
    require(hash(source.sha256) && source.size > 0 &&
                paths.insert(source.relative_path).second,
            "Invalid or duplicate navmesh source identity");
  }
  auto own = "maps/" + m.map + ".unr";
  require(paths.count(own) != 0, "Navmesh own map source missing");
  for (const auto &name : m.loaded_neighbors)
    require(paths.count("maps/" + name + ".unr") != 0,
            "Navmesh loaded neighbor source missing");
}
} // namespace
glm::dvec4 region_bounds(const std::string &map) {
  require(map.size() == 5 && map[2] == '_' &&
              map.find_first_not_of("0123456789_") == std::string::npos &&
              map[0] != '_' && map[1] != '_' && map[3] != '_' && map[4] != '_',
          "Invalid navmesh region name");
  const double x = (std::stoi(map.substr(0, 2)) - 20) * 32768.0,
               y = (std::stoi(map.substr(3, 2)) - 18) * 32768.0;
  return {x, y, x + 32768, y + 32768};
}
std::vector<std::string> region_neighbors(const std::string &map) {
  region_bounds(map);
  const int x = std::stoi(map.substr(0, 2)), y = std::stoi(map.substr(3, 2));
  std::vector<std::string> out;
  auto two = [](int n) { return (n < 10 ? "0" : "") + std::to_string(n); };
  for (int dx = -1; dx <= 1; ++dx)
    for (int dy = -1; dy <= 1; ++dy)
      if ((dx || dy) && x + dx >= 0 && x + dx <= 99 && y + dy >= 0 &&
          y + dy <= 99)
        out.push_back(two(x + dx) + "_" + two(y + dy));
  return out;
}
territory::Json region_profile(const Settings &s) {
  validate_region_grid(s);
  return {{"actor_height", s.actor_height},
          {"actor_radius", s.actor_radius},
          {"max_climb", s.max_climb},
          {"max_slope", s.max_slope},
          {"cell_size", s.cell_size},
          {"cell_height", s.cell_height},
          {"tile_cells", s.tile_cells},
          {"tile_width", double(s.cell_size) * s.tile_cells},
          {"origin", {0, 0, 0}},
          {"region_size", 32768},
          {"axes", "x,z,y"},
          {"scale", 1},
          {"container", "MSET-1/Detour-7/LE/ref64/nvp6"},
          {"recast_revision", "c187b7e+libs/patches/recast.patch"},
          {"builder_revision", "neighbor-context-1"}};
}
std::filesystem::path metadata_path(const std::filesystem::path &mesh) {
  auto p = mesh;
  p += ".json";
  return p;
}
RegionMetadata read_region_metadata(const std::filesystem::path &file) {
  const auto json = territory::read_json(metadata_path(file));
  require(json.at("schema") == 1, "Unsupported navmesh metadata schema");
  RegionMetadata m;
  m.map = json.at("map").get<std::string>();
  m.generator = json.at("generator").get<std::string>();
  const auto &p = json.at("profile");
  require(p.at("tile_cells").is_number_integer() &&
              p.at("tile_cells").get<double>() >= 8 &&
              p.at("tile_cells").get<double>() <= 256,
          "Invalid navmesh tile cells");
  m.settings = {
      p.at("actor_height").get<float>(), p.at("actor_radius").get<float>(),
      p.at("max_climb").get<float>(),    p.at("max_slope").get<float>(),
      p.at("cell_size").get<float>(),    p.at("cell_height").get<float>(),
      p.at("tile_cells").get<int>()};
  require(p == region_profile(m.settings),
          "Unsupported navmesh profile, grid or library revision");
  m.padding = json.at("padding").get<double>();
  m.neighbor_context = json.at("neighbor_context").get<bool>();
  m.loaded_neighbors =
      json.at("loaded_neighbors").get<std::vector<std::string>>();
  m.missing_neighbors =
      json.at("missing_neighbors").get<std::vector<std::string>>();
  const auto &mesh = json.at("mesh");
  require(mesh.at("file") == territory::path_utf8(file.filename()),
          "Navmesh sidecar names a different file");
  require(mesh.at("size").is_number_unsigned(), "Invalid navmesh mesh size");
  m.mesh = {file, mesh.at("size").get<std::uint64_t>(),
            mesh.at("sha256").get<std::string>()};
  require(json.at("sources").is_array() && json.at("sources").size() <= 100000,
          "Invalid navmesh source list");
  for (const auto &source : json.at("sources")) {
    require(source.at("size").is_number_unsigned(), "Invalid source size");
    m.sources.push_back({source.at("path").get<std::string>(),
                         source.at("sha256").get<std::string>(),
                         source.at("size").get<std::uint64_t>()});
  }
  validate_metadata(m);
  const auto actual = territory::file_identity(file);
  require(actual.bytes == m.mesh.bytes && actual.sha256 == m.mesh.sha256,
          "Navmesh bytes do not match sidecar");
  return m;
}
void write_region_metadata_new(const RegionMetadata &m,
                               const std::filesystem::path &file,
                               const Cancel &cancel) {
  if (cancel && cancel())
    throw Cancelled{};
  validate_metadata(m);
  const auto actual = territory::file_identity(file);
  require(actual.bytes == m.mesh.bytes && actual.sha256 == m.mesh.sha256,
          "Navmesh changed before metadata publication");
  auto sources = territory::Json::array();
  for (const auto &source : m.sources)
    sources.push_back({{"path", source.relative_path},
                       {"sha256", source.sha256},
                       {"size", source.size}});
  if (cancel && cancel())
    throw Cancelled{};
  territory::write_json_atomic(
      metadata_path(file),
      {{"schema", 1},
       {"map", m.map},
       {"generator", m.generator},
       {"profile", region_profile(m.settings)},
       {"padding", m.padding},
       {"neighbor_context", m.neighbor_context},
       {"loaded_neighbors", m.loaded_neighbors},
       {"missing_neighbors", m.missing_neighbors},
       {"sources", sources},
       {"mesh",
        {{"file", territory::path_utf8(file.filename())},
         {"size", m.mesh.bytes},
         {"sha256", m.mesh.sha256}}}},
      false);
}
} // namespace navmesh
