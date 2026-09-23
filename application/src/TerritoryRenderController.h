#pragma once
#include "TerritoryProcess.h"
#include <chrono>
#include <territory/Job.h>
class TerritoryRenderController {
public:
  using Clock = std::function<std::chrono::steady_clock::time_point()>;
  TerritoryRenderController(std::filesystem::path exe,
                            std::unique_ptr<TerritoryProcess>,
                            Clock = std::chrono::steady_clock::now);
  ~TerritoryRenderController();
  bool start(const std::filesystem::path &client,
             const std::vector<std::string> &maps, const territory::Settings &,
             const std::filesystem::path &output,
             territory::Mode mode = territory::Mode::Render);
  void poll();
  void cancel();
  bool active() const;
  const std::optional<territory::Status> &status() const;
  const std::filesystem::path &job_directory() const;
  const std::string &error() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl;
};
