#include "PathfindingResults.h"
#include "Support.h"
#include <limits>
#include <locale>
#include <sstream>

using namespace pf_test;
using namespace pathfinding;

int result_presentation_tests() {
  int n = 0;
  n += check("benchmark headline uses median rather than last query latency", [] {
    RouteResult r;
    r.metrics={{"search_ns",9000000},{"server_budget_ms",1},
      {"benchmark",{{"search_ns",{{"median_ns",1500000}}}}}};
    auto s=summarize_route(r);
    return s.search_ms==1.5 && s.budget_ratio==1.5;
  });
  n += check("server budget ratio compares only measured search time", [] {
    RouteResult r;
    r.metrics = {{"search_ns", 347000000},
                 {"server_budget_ms", 100},
                 {"backend_wall_ns", 60000000000ull}};
    const auto summary = summarize_route(r);
    r.metrics["search_ns"] = nullptr;
    return summary.budget_ratio &&
           std::abs(*summary.budget_ratio - 3.47) < 1e-9 &&
           !summarize_route(r).budget_ratio;
  });
  n += check(
      "nanoseconds convert to milliseconds without adding overlapping scopes",
      [] {
        const Json metrics{{"search_ns", 1250000},
                           {"load_ns", 9000000},
                           {"backend_wall_ns", 70000000}};
        return route_metric_ms(metrics, "search_ns") == 1.25 &&
               route_metric_ms(metrics, "load_ns") == 9.;
      });
  n += check("missing null and invalid timer values remain unavailable while "
             "zero is measured",
             [] {
               const Json metrics{
                   {"search_ns", nullptr},
                   {"load_ns", -1},
                   {"snap_ns", "10"},
                   {"smoothing_ns", 0},
                   {"validation_ns", std::numeric_limits<double>::infinity()}};
               return !route_metric_ms(metrics, "search_ns") &&
                      !route_metric_ms(metrics, "startup_ns") &&
                      !route_metric_ms(metrics, "load_ns") &&
                      !route_metric_ms(metrics, "snap_ns") &&
                      !route_metric_ms(metrics, "validation_ns") &&
                      route_metric_ms(metrics, "smoothing_ns") == 0.;
             });
  n += check(
      "running idle and stale outcomes cannot be mistaken for a current result",
      [] {
        return route_result_freshness(false, false, 2, 0) ==
                   ResultFreshness::Idle &&
               route_result_freshness(true, false, 2, 1) ==
                   ResultFreshness::Running &&
               route_result_freshness(true, true, 2, 1) ==
                   ResultFreshness::Running &&
               route_result_freshness(false, true, 2, 1) ==
                   ResultFreshness::Stale &&
               route_result_freshness(false, true, 2, 2) ==
                   ResultFreshness::Current;
      });
  n += check(
      "only reached is green and incomplete unknown cancelled outcomes are "
      "warnings",
      [] {
        for (auto status : {RouteStatus::Partial, RouteStatus::UnknownLegacy,
                            RouteStatus::ResourceLimit, RouteStatus::Cancelled})
          if (route_result_tone(status) != ResultTone::Warning)
            return false;
        for (auto status :
             {RouteStatus::NoPath, RouteStatus::InvalidEndpoint,
              RouteStatus::LayerMismatch, RouteStatus::InvalidData,
              RouteStatus::BackendUnavailable, RouteStatus::Failed})
          if (route_result_tone(status) != ResultTone::Error)
            return false;
        return route_result_tone(RouteStatus::Reached) == ResultTone::Success;
      });
  n += check("summary uses final game XYZ polyline with no phantom length for "
             "an absent route",
             [] {
               RouteResult result;
               result.status = RouteStatus::Partial;
               result.metrics = {{"search_ns", 250000}, {"total_ns", 60000000}};
               result.raw = {{0, 0, 0}, {1000, 1000, 1000}};
               result.final_path = {{-5, 7, 9}, {-2, 11, 9}, {-2, 11, 21}};
               const auto s = summarize_route(result);
               result.final_path.clear();
               const auto empty = summarize_route(result);
               return s.search_ms == .25 && s.points == 3 &&
                      s.length_xyz == 17. && s.tone == ResultTone::Warning &&
                      !empty.length_xyz && empty.points == 0;
             });
  n +=
      check("CSV exports final points in game XYZ order and preserves "
            "fractional coordinates",
            [] {
              RouteResult result;
              result.raw = {{999, 888, 777}};
              result.final_path = {{166136.25, 53528.5, -3352.125}, {-1, 0, 3}};
              return final_route_csv(result) ==
                     "x,y,z\n166136.25,53528.5,-3352.125\n-1,0,3\n";
            });
  n += check(
      "CSV rejects absent and nonfinite final routes before creating a file",
      [] {
        TempDirectory temporary;
        RouteResult result;
        const auto path = temporary.path / "route.csv";
        const bool empty_rejected =
            rejects([&] { export_final_route_new(result, path); });
        result.final_path = {{std::numeric_limits<double>::quiet_NaN(), 1, 2}};
        return empty_rejected &&
               rejects([&] { export_final_route_new(result, path); }) &&
               !std::filesystem::exists(path);
      });
  n += check("export writes a new CSV and a repeated export leaves existing "
             "bytes untouched",
             [] {
               TempDirectory temporary;
               RouteResult result;
               result.final_path = {{12.5, -3, 7}};
               const auto path = temporary.path / "route.csv";
               export_final_route_new(result, path);
               result.final_path = {{9, 9, 9}};
               const bool rejected =
                   rejects([&] { export_final_route_new(result, path); });
               std::ifstream stream(path, std::ios::binary);
               const std::string bytes{std::istreambuf_iterator<char>(stream),
                                       {}};
               return rejected && bytes == "x,y,z\n12.5,-3,7\n";
             });
  return n;
}

#ifdef PATHFINDING_RESULTS_TEST_MAIN
int main() { return result_presentation_tests() ? 1 : 0; }
#endif
