#pragma once
#include "Job.h"
#include "TerritoryRenderer.h"
namespace territory {
struct RunResult {
  Status status;
  std::filesystem::path report;
  int exit_code{};
};
struct RunnerServices {
  std::function<VisualScene(const std::filesystem::path &, const std::string &,
                            const Cancel &)>
      load;
  std::function<RasterInfo(const VisualScene &, const RasterSettings &,
                           const Cancel &, const TileProgress &, const Rows &)>
      render;
  // Optional lifetime observer, called only after the owned scene is destroyed.
  std::function<void()> scene_released;
};
RunnerServices default_runner_services();
RunResult run_job(const Job &, const Progress &, const Cancel &,
                  const RunnerServices &);
int run_job_file(const std::filesystem::path &job_directory);
Json result_json(const Job &, const RunResult &);
} // namespace territory
