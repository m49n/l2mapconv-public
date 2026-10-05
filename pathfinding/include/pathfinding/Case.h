#pragma once
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <territory/Json.h>
#include <vector>

namespace pathfinding {
using Json = territory::Json;
struct WorldPoint {
  double x{}, y{}, z{};
  bool operator==(const WorldPoint &) const = default;
};
struct Endpoint {
  WorldPoint requested;
  double confirmed_z{};
  std::map<std::string, double> backend_z{};
};
struct FileIdentity {
  std::filesystem::path path;
  std::string sha256;
  std::uint64_t size{};
};
struct SearchSettings {
  int server_budget_ms{100}; // Reference only; never stops the experiment.
  int timeout_ms{5000};
  int nav_max_nodes{65535}; // Expanded A* nodes, not allocated memory.
  int l2j_max_window_cells{512};
};
struct NavRegionInput { std::string map; FileIdentity mesh, metadata; };
struct RouteCase {
  std::string id, map;
  Endpoint a, b;
  FileIdentity l2j, navmesh;
  double snap_horizontal{8}, snap_vertical{32};
  std::map<std::string, std::string> expected_backends{};
  SearchSettings search{};
  std::vector<NavRegionInput> nav_regions{};
};
struct BackendProfile {
  std::filesystem::path java;
  std::vector<std::filesystem::path> legacy_classpath;
  std::filesystem::path legacy_config, nav_runner_directory, detour_jar;
  bool legacy_experimental_limits{};
};

bool has_input(const FileIdentity &);
std::vector<std::string> enabled_backends(const RouteCase &);
std::vector<std::string> navigation_regions(const RouteCase &);
std::vector<NavRegionInput> navmesh_region_inputs(const RouteCase &);
bool backend_scope_supported(const RouteCase &, const std::string &);
void validate_search_settings(const SearchSettings &);
Json search_settings_json(const SearchSettings &);
RouteCase case_for_backend(const RouteCase &, const std::string &backend);

void validate_point(WorldPoint);
WorldPoint point_from_json(const Json &);
Json point_json(WorldPoint);
// Lower inclusive XY corner of a standard Lineage II region.
WorldPoint region_origin(const std::string &map);
void validate_case(const RouteCase &);
Json identity_json(const FileIdentity &);
FileIdentity identity_from_json(const Json &);
FileIdentity identify_file(const std::filesystem::path &);
void verify_identity(const FileIdentity &);
Json case_json(const RouteCase &);
RouteCase case_from_json(const Json &);
RouteCase read_case(const std::filesystem::path &);
void write_case_new(const RouteCase &, const std::filesystem::path &);
Json profile_json(const BackendProfile &);
BackendProfile profile_from_json(const Json &);
// File profiles may use paths relative to their own directory. Job profiles
// remain absolute after loading and retain the existing schema-1 contract.
BackendProfile read_profile(const std::filesystem::path &);
std::filesystem::path default_backend_profile_path(
    const std::filesystem::path &executable_directory,
    const std::filesystem::path &selected_override = {});
std::vector<FileIdentity> input_changes(const RouteCase &);
RouteCase with_current_identities(const RouteCase &);
void verify_backend_identity(const RouteCase &, const std::string &backend,
                             const std::string &actual_sha256);
} // namespace pathfinding
