#pragma once
#include <optional>
#include <territory/Job.h>
struct TerritoryCommand {
  territory::Mode mode{territory::Mode::Render};
  std::filesystem::path client, output;
  std::vector<std::string> maps;
  territory::Settings settings;
};
std::optional<TerritoryCommand>
parse_territory_command(const std::vector<std::string> &args);
int run_territory_command(const TerritoryCommand &);
std::optional<int>
dispatch_territory_command(const std::vector<std::string> &args);
std::vector<std::string> territory_utf8_arguments(int argc, char **argv);
