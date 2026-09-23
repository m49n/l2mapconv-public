#include "TerritoryCommandLine.h"
#include "TestSupport.h"
#include <iostream>
#include <sstream>
#include <territory/PathIO.h>
namespace {
template <class Fn> bool throws(Fn fn) {
  try {
    fn();
    return false;
  } catch (const std::exception &) {
    return true;
  }
}
} // namespace
int cli_tests() {
  int failures = 0;
  std::vector<std::string> base{"app",           "--render-territory",
                                "--client-root", "C:/client/sam",
                                "--output",      "C:/Result"};
  auto args = base;
  args.insert(args.end(), {"--", "24_18", "22_22", "22_22"});
  auto c = parse_territory_command(args);
  failures += expect(c && c->settings.resolution == 8192 && c->settings.water &&
                         c->maps == std::vector<std::string>{"22_22", "24_18"},
                     "render defaults and unique sorted maps");
  for (auto resolution : {"4096", "8192", "16384"}) {
    auto a = base;
    a.insert(a.end(),
             {"--resolution", resolution, "--no-water", "--", "22_22"});
    auto command = parse_territory_command(a);
    failures += expect(
        command && command->settings.resolution == std::stoi(resolution) &&
            !command->settings.water,
        "explicit render settings");
  }
  args = base;
  args[1] = "--inspect-territory";
  args.insert(args.end(), {"--", "22_22"});
  c = parse_territory_command(args);
  failures += expect(c && c->mode == territory::Mode::Inspect, "inspect mode");
  for (auto extra :
       std::vector<std::vector<std::string>>{{"--build"},
                                             {"--preview"},
                                             {"--bad"},
                                             {"--render-territory"},
                                             {"--resolution", "512"},
                                             {"--resolution", "8192x"},
                                             {"--output", "again"},
                                             {"22_22"},
                                             {"--", "../22_22"}}) {
    auto a = base;
    a.insert(a.end(), extra.begin(), extra.end());
    failures += expect(throws([&] { parse_territory_command(a); }),
                       "reject ambiguous/malformed/unknown CLI input");
  }
  for (auto bad : std::vector<std::vector<std::string>>{
           {"app", "--render-territory", "--client-root", "x", "--", "22_22"},
           {"app", "--render-territory", "--output", "x", "--", "22_22"},
           base,
           {"app", "--inspect-territory", "--resolution", "8192"},
           {"app", "--inspect-territory", "--no-water"}})
    failures += expect(throws([&] { parse_territory_command(bad); }),
                       "missing fields or render options on inspect reject");
  failures +=
      expect(!parse_territory_command(
                 {"app", "--build", "--client-root", "x", "--", "22_22"}),
             "legacy routing unchanged");
  std::ostringstream output;
  auto *previous = std::cout.rdbuf(output.rdbuf());
  auto code = dispatch_territory_command(
      {"app", "--render-territory", "--bad\"option"});
  std::cout.rdbuf(previous);
  const auto error = territory::Json::parse(output.str());
  failures +=
      expect(code == 2 && error["exit_code"] == 2 &&
                 error["state"] == "failed" && error["error"].is_string(),
             "malformed CLI emits exactly one escaped JSON error");
  return failures;
}
