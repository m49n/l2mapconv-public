#pragma once
#include <pathfinding/Result.h>

class PathfindingController;
struct PathfindingContext;

enum class ResultTone { Success, Warning, Error, Neutral };
enum class ResultFreshness { Idle, Running, Current, Stale };
struct RouteSummary {
  std::optional<double> search_ms, length_xyz, budget_ratio;
  std::size_t points{};
  ResultTone tone{ResultTone::Neutral};
};

std::optional<double> route_metric_ms(const pathfinding::Json &,
                                      const char *name);
ResultTone route_result_tone(pathfinding::RouteStatus);
ResultFreshness route_result_freshness(bool running, bool has_results,
                                       std::uint64_t current_revision,
                                       std::uint64_t submitted_revision);
RouteSummary summarize_route(const pathfinding::RouteResult &);
std::string final_route_csv(const pathfinding::RouteResult &);
void export_final_route_new(const pathfinding::RouteResult &,
                            const std::filesystem::path &);

struct PathfindingResultsState {
  std::string request_id, action_message;
  std::vector<RouteSummary> summaries;
  bool action_failed{};
};
// Place above a separately scrolling controls child to keep outcomes visible.
void draw_pathfinding_results(const PathfindingContext &,
                              const PathfindingController &,
                              PathfindingResultsState &);
