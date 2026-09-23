#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <territory/Diagnostics.h>
#include <territory/Job.h>
#include <territory/PathIO.h>
namespace territory {
namespace {
constexpr std::array<std::string_view, 9> phases = {"idle",
                                                    "preparing",
                                                    "loading",
                                                    "rendering",
                                                    "saving",
                                                    "completed",
                                                    "completed_with_warnings",
                                                    "failed",
                                                    "cancelled"};
constexpr std::array<std::string_view, 6> issue_names = {
    "missing_package", "missing_object", "unsupported",
    "corrupt",         "simplified",     "water_unresolved"};
bool map_name(std::string_view s) {
  return s.size() == 5 && s[2] == '_' && s[0] >= '0' && s[0] <= '9' &&
         s[1] >= '0' && s[1] <= '9' && s[3] >= '0' && s[3] <= '9' &&
         s[4] >= '0' && s[4] <= '9';
}
bool identifier(std::string_view s) {
  return !s.empty() && s.size() <= 128 &&
         std::all_of(s.begin(), s.end(), [](unsigned char c) {
           return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '-' || c == '_';
         });
}
std::string string_field(const Json &j, const char *name) {
  const auto &v = j.at(name);
  if (!v.is_string())
    throw std::invalid_argument(std::string(name) + " must be a string");
  return v.get<std::string>();
}
std::size_t count_field(const Json &j, const char *name) {
  const auto &v = j.at(name);
  if (!v.is_number_integer() ||
      (v.is_number_integer() && !v.is_number_unsigned() &&
       v.get<std::int64_t>() < 0))
    throw std::invalid_argument(std::string(name) +
                                " must be a nonnegative integer");
  const auto n = v.get<std::uint64_t>();
  if (n > std::numeric_limits<std::size_t>::max())
    throw std::invalid_argument("counter overflow");
  return static_cast<std::size_t>(n);
}
void schema(const Json &j) {
  if (!j.is_object() || count_field(j, "schema_version") != 1)
    throw std::invalid_argument("unsupported JSON schema");
  // Also validates strings in JSON values constructed directly by callers.
  (void)j.dump();
}
std::vector<std::string> strings(const Json &j, const char *name) {
  if (!j.at(name).is_array())
    throw std::invalid_argument(std::string(name) + " must be an array");
  std::vector<std::string> result;
  for (const auto &v : j.at(name)) {
    if (!v.is_string())
      throw std::invalid_argument("array element must be a string");
    result.push_back(v.get<std::string>());
  }
  return result;
}
void normalize(std::vector<std::string> &maps) {
  std::sort(maps.begin(), maps.end());
  maps.erase(std::unique(maps.begin(), maps.end()), maps.end());
}
bool output_leaf(std::string_view s) {
  const auto dot = s.find('.');
  if (dot == std::string_view::npos || !identifier(s.substr(0, dot)))
    return false;
  return s.substr(dot) == ".png" || s.substr(dot) == ".json";
}
} // namespace
void validate_settings(const Settings &s) {
  if (s.resolution != 4096 && s.resolution != 8192 && s.resolution != 16384)
    throw std::invalid_argument("resolution must be 4096, 8192 or 16384");
}
void validate_job(const Job &j) {
  if (!identifier(j.id))
    throw std::invalid_argument("invalid job identity");
  if (j.client.empty() || !j.client.is_absolute() ||
      !std::filesystem::is_directory(j.client))
    throw std::invalid_argument(
        "client must be an existing absolute directory");
  if (j.directory.empty() || !j.directory.is_absolute() ||
      is_within(j.directory, j.client))
    throw std::invalid_argument(
        "job directory must be absolute and outside client");
  if (j.maps.empty() || j.maps.size() > 10000)
    throw std::invalid_argument("select one or more maps");
  for (const auto &map : j.maps)
    if (!map_name(map))
      throw std::invalid_argument("invalid map name: " + map);
  if (j.mode != Mode::Inspect && j.mode != Mode::Render)
    throw std::invalid_argument("invalid mode");
  validate_settings(j.settings);
}
void check_cancel(const Cancel &c) {
  if (c && c())
    throw Cancelled{};
}
Json to_json(const Job &j) {
  validate_job(j);
  auto maps = j.maps;
  normalize(maps);
  return {{"schema_version", 1},
          {"job_id", j.id},
          {"client_root", path_utf8(j.client)},
          {"maps", maps},
          {"mode", j.mode == Mode::Inspect ? "inspect" : "render"},
          {"resolution", j.settings.resolution},
          {"water", j.settings.water}};
}
Job job_from_json(const Json &value, const std::filesystem::path &directory) {
  schema(value);
  Job j;
  j.directory = directory;
  j.id = string_field(value, "job_id");
  j.client = path_from_utf8(string_field(value, "client_root"));
  j.maps = strings(value, "maps");
  normalize(j.maps);
  const auto mode = string_field(value, "mode");
  if (mode != "inspect" && mode != "render")
    throw std::invalid_argument("invalid job mode");
  j.mode = mode == "inspect" ? Mode::Inspect : Mode::Render;
  auto resolution = count_field(value, "resolution");
  if (resolution > std::numeric_limits<int>::max())
    throw std::invalid_argument("resolution overflow");
  j.settings.resolution = static_cast<int>(resolution);
  if (!value.at("water").is_boolean())
    throw std::invalid_argument("water must be boolean");
  j.settings.water = value.at("water").get<bool>();
  validate_job(j);
  return j;
}
std::string_view phase_name(Phase p) {
  auto i = static_cast<std::size_t>(p);
  if (i >= phases.size())
    throw std::invalid_argument("invalid job phase");
  return phases[i];
}
Json to_json(const Status &s) {
  return {{"schema_version", 1},
          {"job_id", s.job_id},
          {"state", phase_name(s.phase)},
          {"map", s.map},
          {"error", s.error},
          {"map_index", s.map_index},
          {"map_count", s.map_count},
          {"tiles_done", s.tiles_done},
          {"tiles_total", s.tiles_total},
          {"warnings", s.warnings},
          {"files", s.files}};
}
Status status_from_json(const Json &value, std::string_view expected) {
  schema(value);
  Status s;
  s.job_id = string_field(value, "job_id");
  if (!identifier(s.job_id) || s.job_id != expected)
    throw std::invalid_argument("foreign status identity");
  auto state = string_field(value, "state");
  const auto it = std::find(phases.begin(), phases.end(), state);
  if (it == phases.end())
    throw std::invalid_argument("invalid status state");
  s.phase = static_cast<Phase>(it - phases.begin());
  s.map = string_field(value, "map");
  s.error = string_field(value, "error");
  if (!s.map.empty() && !map_name(s.map))
    throw std::invalid_argument("invalid status map");
  s.map_index = count_field(value, "map_index");
  s.map_count = count_field(value, "map_count");
  s.tiles_done = count_field(value, "tiles_done");
  s.tiles_total = count_field(value, "tiles_total");
  s.warnings = count_field(value, "warnings");
  if (s.map_index > s.map_count || s.tiles_done > s.tiles_total)
    throw std::invalid_argument("inconsistent progress counters");
  s.files = strings(value, "files");
  for (const auto &f : s.files)
    if (!output_leaf(f))
      throw std::invalid_argument("invalid output filename");
  return s;
}
Json to_json(const Report &r, std::string_view id) {
  if (!identifier(id) || !r.maps.is_array())
    throw std::invalid_argument("invalid report");
  Json issues = Json::array();
  for (const auto &issue : r.issues) {
    auto kind = static_cast<std::size_t>(issue.kind);
    if (kind >= issue_names.size())
      throw std::invalid_argument("invalid issue kind");
    issues.push_back({{"kind", issue_names[kind]},
                      {"source", issue.source},
                      {"reason", issue.reason},
                      {"surfaces", issue.surfaces}});
  }
  return {{"schema_version", 1},
          {"job_id", id},
          {"maps", r.maps},
          {"issues", issues}};
}
} // namespace territory
