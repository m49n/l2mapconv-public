#include "TerritoryCommandLine.h"
#include <algorithm>
#include <atomic>
#include <charconv>
#include <iostream>
#include <set>
#include <territory/JobRunner.h>
#include <territory/PathIO.h>
#include <utils/Log.h>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>

#include <shellapi.h>
#endif
namespace {
bool public_mode(std::string_view value) {
  return value == "--render-territory" || value == "--inspect-territory";
}
bool numeric_map(std::string_view value) {
  return value.size() == 5 && value[2] == '_' &&
         std::all_of(
             value.begin(), value.end(),
             [](char c) { return c == '_' || (c >= '0' && c <= '9'); }) &&
         value.find('_') == 2 && value.rfind('_') == 2;
}
int input_error(const std::string &error) {
  territory::Status status;
  status.phase = territory::Phase::Failed;
  status.error = error;
  auto json = territory::to_json(status);
  json["exit_code"] = 2;
  json["output_directory"] = nullptr;
  json["report"] = nullptr;
  std::cout << json.dump() << '\n';
  return 2;
}
struct LogRedirect {
  std::streambuf *old = std::cout.rdbuf(std::cerr.rdbuf());
  bool colored = utils::Log::colored;
  LogRedirect() { utils::Log::colored = false; }
  ~LogRedirect() {
    std::cout.flush();
    std::cout.rdbuf(old);
    utils::Log::colored = colored;
  }
};
#ifdef _WIN32
std::atomic_bool interrupted{};
BOOL WINAPI interrupt(DWORD signal) {
  if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT) {
    interrupted = true;
    return TRUE;
  }
  return FALSE;
}
struct InterruptScope {
  InterruptScope() {
    interrupted = false;
    SetConsoleCtrlHandler(interrupt, TRUE);
  }
  ~InterruptScope() { SetConsoleCtrlHandler(interrupt, FALSE); }
};
#endif
} // namespace
std::optional<TerritoryCommand>
parse_territory_command(const std::vector<std::string> &args) {
  if (std::none_of(args.begin() + std::min<std::size_t>(1, args.size()),
                   args.end(), public_mode))
    return std::nullopt;
  TerritoryCommand result;
  std::set<std::string> seen;
  int modes = 0;
  bool maps = false;
  for (std::size_t n = 1; n < args.size(); ++n) {
    const auto &arg = args[n];
    if (maps) {
      if (!numeric_map(arg))
        throw std::invalid_argument("Expected map name dd_dd after --: " + arg);
      result.maps.push_back(arg);
      continue;
    }
    if (arg == "--") {
      maps = true;
      continue;
    }
    if (!seen.insert(arg).second)
      throw std::invalid_argument("Duplicate option: " + arg);
    if (public_mode(arg)) {
      ++modes;
      result.mode = arg == "--inspect-territory" ? territory::Mode::Inspect
                                                 : territory::Mode::Render;
      continue;
    }
    if (arg == "--no-water") {
      result.settings.water = false;
      continue;
    }
    if (arg != "--client-root" && arg != "--output" && arg != "--resolution")
      throw std::invalid_argument("Unknown or incompatible option: " + arg);
    if (++n == args.size() || args[n].starts_with("--") || args[n].empty())
      throw std::invalid_argument("Missing value: " + arg);
    if (arg == "--client-root")
      result.client = territory::path_from_utf8(args[n]);
    else if (arg == "--output")
      result.output = territory::path_from_utf8(args[n]);
    else {
      const auto &text = args[n];
      int value = 0;
      auto [end, error] =
          std::from_chars(text.data(), text.data() + text.size(), value);
      if (error != std::errc{} || end != text.data() + text.size())
        throw std::invalid_argument("Invalid resolution");
      result.settings.resolution = value;
    }
  }
  if (modes != 1)
    throw std::invalid_argument("Choose exactly one territory mode");
  if (result.mode == territory::Mode::Inspect &&
      (seen.contains("--resolution") || seen.contains("--no-water")))
    throw std::invalid_argument("Inspect does not accept raster options");
  if (result.client.empty() || result.output.empty() || result.maps.empty())
    throw std::invalid_argument(
        "Required: --client-root <sam> --output <directory> -- dd_dd [...]");
  territory::validate_settings(result.settings);
  std::sort(result.maps.begin(), result.maps.end());
  result.maps.erase(std::unique(result.maps.begin(), result.maps.end()),
                    result.maps.end());
  return result;
}
int run_territory_command(const TerritoryCommand &command) {
  try {
    territory::Job job{"pending",
                       std::filesystem::absolute(command.output) / "pending",
                       std::filesystem::absolute(command.client),
                       command.maps,
                       command.mode,
                       command.settings};
    territory::validate_job(job);
    job.directory = territory::create_job_directory(command.output, job.client);
    job.id = territory::path_utf8(job.directory.filename());
    territory::RunResult result;
    {
      LogRedirect logs;
#ifdef _WIN32
      InterruptScope handler;
      result = territory::run_job(
          job, {}, [] { return interrupted.load(); },
          territory::default_runner_services());
#else
      result =
          territory::run_job(job, {}, {}, territory::default_runner_services());
#endif
    }
    std::cout << territory::result_json(job, result).dump() << '\n';
    return result.exit_code;
  } catch (const std::exception &error) {
    return input_error(error.what());
  }
}
std::optional<int>
dispatch_territory_command(const std::vector<std::string> &args) {
  try {
    if (std::find(args.begin(), args.end(), "--render-job") != args.end()) {
      if (args.size() != 3 || args[1] != "--render-job")
        throw std::invalid_argument(
            "Internal mode requires only --render-job <absolute-directory>");
      return territory::run_job_file(territory::path_from_utf8(args[2]));
    }
    if (auto command = parse_territory_command(args))
      return run_territory_command(*command);
    return std::nullopt;
  } catch (const std::exception &error) {
    return input_error(error.what());
  }
}
std::vector<std::string> territory_utf8_arguments(int argc, char **argv) {
#ifdef _WIN32
  (void)argc;
  (void)argv;
  int count = 0;
  auto wide = CommandLineToArgvW(GetCommandLineW(), &count);
  if (!wide)
    throw std::runtime_error("Unable to decode command line");
  struct Free {
    wchar_t **value;
    ~Free() { LocalFree(value); }
  } free{wide};
  std::vector<std::string> result;
  for (int n = 0; n < count; ++n)
    result.push_back(territory::path_utf8(std::filesystem::path(wide[n])));
  return result;
#else
  return {argv, argv + argc};
#endif
}
