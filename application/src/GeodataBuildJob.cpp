#include "GeodataBuildJob.h"
#include <navmesh/InputGeometry.h>

#include <algorithm>
#include <cmath>
#include <territory/PathIO.h>

void validate_geodata_settings(const geodata::BuilderSettings &s) {
  for (float value : {s.actor_height, s.actor_radius, s.max_walkable_angle,
                      s.min_walkable_climb, s.max_walkable_climb, s.cell_size,
                      s.cell_height}) {
    if (!std::isfinite(value))
      throw std::invalid_argument("Geodata settings must be finite numbers");
  }
  if (s.cell_size != 16.0f)
    throw std::invalid_argument("Cell Size must be 16 for full client regions");
  if (s.actor_height <= 0 || s.cell_height <= 0 || s.actor_radius < 0 ||
      s.min_walkable_climb < 0 || s.max_walkable_climb < s.min_walkable_climb ||
      s.max_walkable_angle < 0 || s.max_walkable_angle >= 90)
    throw std::invalid_argument(
        "Use positive heights, nonnegative radius/climbs, "
        "min climb <= max climb and angle 0..<90");
  if (s.actor_height / s.cell_height > 65535 ||
      s.max_walkable_climb / s.cell_height > 65535 || s.actor_radius > 32768)
    throw std::invalid_argument(
        "Actor dimensions exceed the region heightfield");
}

void validate_geodata_job(const GeodataBuildJob &job) {
  territory::validate_job({job.id,
                           job.directory,
                           job.client,
                           job.maps,
                           territory::Mode::Inspect,
                           {}});
  if (!job.l2j && !job.navmesh)
    throw std::invalid_argument("Select L2J, Navmesh or both");
  if (job.client_dat && !job.l2j)
    throw std::invalid_argument("Client DAT requires L2J generation");
  if (job.l2j)
    validate_geodata_settings(job.settings);
  if (job.navmesh)
    navmesh::validate(job.navmesh_settings);
  if (job.navmesh && job.navmesh_neighbor_context)
    navmesh::validate_region_grid(job.navmesh_settings);
}

territory::Json geodata_job_json(const GeodataBuildJob &job) {
  validate_geodata_job(job);
  auto maps = job.maps;
  std::sort(maps.begin(), maps.end());
  maps.erase(std::unique(maps.begin(), maps.end()), maps.end());
  const auto &s = job.settings;
  const auto &n = job.navmesh_settings;
  return {{"schema_version", 2},
          {"kind", "geodata"},
          {"job_id", job.id},
          {"client_root", territory::path_utf8(job.client)},
          {"maps", maps},
          {"navmesh_neighbor_context", job.navmesh_neighbor_context},
          {"outputs",
           {{"l2j", job.l2j},
            {"client_dat", job.client_dat},
            {"navmesh", job.navmesh}}},
          {"navmesh_settings",
           {{"actor_height", n.actor_height},
            {"actor_radius", n.actor_radius},
            {"max_climb", n.max_climb},
            {"max_slope", n.max_slope},
            {"cell_size", n.cell_size},
            {"cell_height", n.cell_height},
            {"tile_cells", n.tile_cells}}},
          {"settings",
           {{"actor_height", s.actor_height},
            {"actor_radius", s.actor_radius},
            {"max_walkable_angle", s.max_walkable_angle},
            {"min_walkable_climb", s.min_walkable_climb},
            {"max_walkable_climb", s.max_walkable_climb},
            {"cell_size", s.cell_size},
            {"cell_height", s.cell_height}}}};
}

GeodataBuildJob geodata_job_from_json(const territory::Json &json,
                                      const std::filesystem::path &directory) {
  if (!json.at("schema_version").is_number_integer() ||
      (json.at("schema_version") != 1 && json.at("schema_version") != 2) ||
      json.at("kind") != "geodata")
    throw std::invalid_argument("Unsupported geodata request");
  GeodataBuildJob job;
  job.id = json.at("job_id").get<std::string>();
  job.directory = directory;
  job.client =
      territory::path_from_utf8(json.at("client_root").get<std::string>());
  job.maps = json.at("maps").get<std::vector<std::string>>();
  job.navmesh_neighbor_context = json.value("navmesh_neighbor_context", false);
  std::sort(job.maps.begin(), job.maps.end());
  job.maps.erase(std::unique(job.maps.begin(), job.maps.end()), job.maps.end());
  if (json.at("schema_version") == 1) {
    job.client_dat = json.at("client_dat").get<bool>();
  } else {
    const auto &o = json.at("outputs");
    job.l2j = o.at("l2j").get<bool>();
    job.client_dat = o.at("client_dat").get<bool>();
    job.navmesh = o.at("navmesh").get<bool>();
    if (job.navmesh) {
      const auto &n = json.at("navmesh_settings");
      if (!n.at("tile_cells").is_number_integer() ||
          n.at("tile_cells").get<double>() < 0 ||
          n.at("tile_cells").get<double>() > 256)
        throw std::invalid_argument(
            "Navmesh tile_cells must be an integer within 8..256");
      job.navmesh_settings = {
          n.at("actor_height").get<float>(), n.at("actor_radius").get<float>(),
          n.at("max_climb").get<float>(),    n.at("max_slope").get<float>(),
          n.at("cell_size").get<float>(),    n.at("cell_height").get<float>(),
          n.at("tile_cells").get<int>()};
    }
  }
  if (job.l2j) {
    const auto &s = json.at("settings");
    job.settings =
        geodata::BuilderSettings{s.at("actor_height").get<float>(),
                                 s.at("actor_radius").get<float>(),
                                 s.at("max_walkable_angle").get<float>(),
                                 s.at("min_walkable_climb").get<float>(),
                                 s.at("max_walkable_climb").get<float>(),
                                 s.at("cell_size").get<float>(),
                                 s.at("cell_height").get<float>()};
  }
  validate_geodata_job(job);
  return job;
}

std::vector<std::string> geodata_output_names(const GeodataBuildJob &job) {
  std::vector<std::string> names;
  for (const auto &map : job.maps) {
    if (job.l2j)
      names.push_back(map + ".l2j");
    if (job.client_dat)
      names.push_back(map + "_conv.dat");
    if (job.navmesh)
      names.push_back(map + ".navmesh");
    if (job.navmesh && job.navmesh_neighbor_context)
      names.push_back(map + ".navmesh.json");
  }
  return names;
}
