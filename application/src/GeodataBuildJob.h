#pragma once

#include <geodata/BuilderSettings.h>
#include <navmesh/Settings.h>
#include <territory/Job.h>

struct GeodataBuildJob {
  std::string id;
  std::filesystem::path directory, client;
  std::vector<std::string> maps;
  geodata::BuilderSettings settings{48, 16, 45.5f, 2, 16, 16, 1};
  bool client_dat{true};
  bool l2j{true}, navmesh{false};
  navmesh::Settings navmesh_settings{};
  bool navmesh_neighbor_context{false};
};

void validate_geodata_settings(const geodata::BuilderSettings &);
void validate_geodata_job(const GeodataBuildJob &);
territory::Json geodata_job_json(const GeodataBuildJob &);
GeodataBuildJob geodata_job_from_json(const territory::Json &,
                                      const std::filesystem::path &);
std::vector<std::string> geodata_output_names(const GeodataBuildJob &);
int run_geodata_job_file(const std::filesystem::path &);
