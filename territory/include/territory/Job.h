#pragma once
#include "Json.h"
#include <exception>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace territory {
enum class Mode { Inspect, Render };
enum class Phase {
  Idle,
  Preparing,
  Loading,
  Rendering,
  Saving,
  Completed,
  CompletedWithWarnings,
  Failed,
  Cancelled
};
struct Settings {
  int resolution{8192};
  bool water{true};
};
struct Job {
  std::string id;
  std::filesystem::path directory, client;
  std::vector<std::string> maps;
  Mode mode{Mode::Render};
  Settings settings;
};
struct Status {
  std::string job_id, map, error;
  Phase phase{Phase::Preparing};
  std::size_t map_index{}, map_count{}, tiles_done{}, tiles_total{}, warnings{};
  std::vector<std::string> files;
};
using Cancel = std::function<bool()>;
using Progress = std::function<void(const Status &)>;
struct Cancelled : std::exception {
  const char *what() const noexcept override { return "cancelled"; }
};
void check_cancel(const Cancel &);
void validate_settings(const Settings &);
void validate_job(const Job &);
Json to_json(const Job &);
Job job_from_json(const Json &, const std::filesystem::path &directory);
Json to_json(const Status &);
Status status_from_json(const Json &, std::string_view expected_job_id);
std::string_view phase_name(Phase);
} // namespace territory
