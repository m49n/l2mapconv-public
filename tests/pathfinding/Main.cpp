#include "PathfindingJob.h"
#include "PathfindingController.h"
#include "PathfindingOverlay.h"
#include <thread>
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <pathfinding/BackendAdapters.h>
#include <pathfinding/Dataset.h>
#include <territory/PathIO.h>
void write_route_fixtures(const std::filesystem::path &);
int contract_tests();
int dataset_tests();
int view_tests();
int input_tests();
int overlay_tests();
int result_presentation_tests();
int replay_tests(const std::filesystem::path &);
int backend_adapter_tests();
int process_tests(const std::filesystem::path &);
int job_tests();
int controller_tests(const std::filesystem::path &,
                     const std::filesystem::path &);
int main(int argc, char **argv) try {
  if (argc == 3 && std::string_view(argv[1]) == "--route-overlay-report") {
    std::ifstream input(argv[2], std::ios::binary);
    const auto report = pathfinding::Json::parse(input);
    if (report.at("results").empty())
      throw std::runtime_error("Report has no routes to verify");
    PathfindingContext context;
    context.show_full_route = true;
    for (const auto &json : report.at("results")) {
      const auto result = pathfinding::result_from_json(json);
      const auto overlay = route_overlay(context, std::span{&result, 1});
      const auto &points = result.final_path;
      if (points.size() < 2 || overlay.segments.size() != points.size() - 1)
        throw std::runtime_error("Displayed route is empty or truncated");
      for (std::size_t i = 0; i < overlay.segments.size(); ++i)
        if (overlay.segments[i].a != points[i] ||
            overlay.segments[i].b != points[i + 1])
          throw std::runtime_error("Displayed route changes the recorded path");
      std::cout << "PASS " << result.backend << " complete overlay: "
                << overlay.segments.size() << " segments\n";
    }
    return 0;
  }
  // Headless acceptance uses the same controller as the UI, including cancel.
  if(argc==8 && std::string_view(argv[1])=="--benchmark-controller") {
    const auto route=pathfinding::read_case(argv[2]);
    const auto profile=pathfinding::read_profile(argv[3]);
    PathfindingController controller(std::filesystem::absolute(argv[0]).parent_path()/"l2mapconv.exe",
                                    std::filesystem::absolute(argv[4]));
    pathfinding::BenchmarkSettings settings{std::stoi(argv[5]),std::stoi(argv[6])};
    const std::string cancel_phase=argv[7];
    if(!controller.start(route,profile,settings))throw std::runtime_error(controller.error());
    bool cancelled=false;
    auto observed=pathfinding::Json::object();
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(3);
    while(controller.active()) {
      controller.poll();
      const auto& p=controller.benchmark_progress();
      if(!p.is_null()) {
        const auto key=p.at("backend").get<std::string>();
        if(observed.contains(key) && observed.at(key).at("pid")!=p.at("pid"))
          throw std::runtime_error("JVM restarted within benchmark");
        observed[key]=p;
        if(!cancelled && p.at("phase")==cancel_phase) {controller.cancel();cancelled=true;}
      }
      if(std::chrono::steady_clock::now()>deadline)throw std::runtime_error("Acceptance wall timeout");
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    territory::write_json_atomic(controller.job_directory()/"observed-progress.json",observed,false);
    for(const auto& r:controller.results()) {
      if(cancelled) {
        if(r.status!=pathfinding::RouteStatus::Cancelled)throw std::runtime_error("Cancelled series returned a result");
      } else if(r.metrics.at("benchmark").at("iterations")!=settings.iterations ||
                r.metrics.at("benchmark_samples").size()!=static_cast<std::size_t>(settings.iterations))
        throw std::runtime_error("Wrong measured count");
    }
    if(controller.results().empty() || (cancel_phase!="none" && !cancelled))
      throw std::runtime_error("Missing results or requested cancellation phase");
    std::cout<<"PASS benchmark controller "<<cancel_phase<<" "<<controller.job_directory()<<'\n';
    return 0;
  }
  if (argc == 7 && std::string_view(argv[1]) == "--candidates") {
    const auto dataset =
        pathfinding::Dataset::load(argv[2], pathfinding::identify_file(argv[3]),
                                   pathfinding::identify_file(argv[4]));
    for (const auto &p :
         dataset.candidates(std::stod(argv[5]), std::stod(argv[6])))
      std::cout << p.surface_id << " z=" << p.resolved.z << " valid=" << p.valid
                << '\n';
    return 0;
  }
  if (argc == 5 && std::string_view(argv[1]) == "--make-job") {
    const auto job = make_pathfinding_job(pathfinding::read_case(argv[2]),
                                          pathfinding::read_profile(argv[3]),
                                          std::filesystem::absolute(argv[4]));
    std::cout << job.directory << '\n';
    return 0;
  }
  if (argc == 4 && std::string_view(argv[1]) == "--legacy-result") {
    std::ifstream in(argv[3], std::ios::binary);
    std::string text{std::istreambuf_iterator<char>(in),
                     std::istreambuf_iterator<char>()};
    const auto r = pathfinding::parse_legacy_jsonl(
        text, pathfinding::read_case(argv[2]), "direct-smoke");
    std::cout << pathfinding::result_json(r).dump(2) << '\n';
    return 0;
  }
  if (argc == 3 && std::string_view(argv[1]) == "--fixtures") {
    write_route_fixtures(argv[2]);
    return 0;
  }
  if (argc == 5 && std::string_view(argv[1]) == "--dataset") {
    const auto d =
        pathfinding::Dataset::load(argv[2], pathfinding::identify_file(argv[3]),
                                   pathfinding::identify_file(argv[4]));
    std::cout << "load_ns=" << d.load_ns() << '\n';
    const auto origin = d.origin();
    const auto start = std::chrono::steady_clock::now();
    const auto bins =
        d.l2j_overview({origin.x, origin.y, origin.x + 32768, origin.y + 32768},
                       -32768, 32768, 256);
    std::cout << "overview_bins=" << bins.size() << " overview_ms="
              << std::chrono::duration<double, std::milli>(
                     std::chrono::steady_clock::now() - start)
                     .count()
              << '\n';
    const auto polygons = d.nav_polygons(-32768, 32768);
    std::size_t edges = 0;
    for (const auto &polygon : polygons)
      edges += polygon.size();
    std::cout << "nav_polygons=" << polygons.size() << " edges=" << edges
              << '\n';
    for (auto p : d.candidates(166136, 53528))
      std::cout << p.surface_id << " z=" << p.resolved.z << " valid=" << p.valid
                << '\n';
    return 0;
  }
  int n = contract_tests() + dataset_tests() + view_tests() + input_tests() +
          overlay_tests() + result_presentation_tests() +
          backend_adapter_tests() + job_tests() +
          replay_tests(std::filesystem::absolute(argv[0]).parent_path() /
                       "l2mapconv.exe") +
          process_tests(std::filesystem::absolute(argv[0]).parent_path() /
                        "pathfinding_process_child.exe") +
          controller_tests(std::filesystem::absolute(argv[0]).parent_path() /
                               "l2mapconv.exe",
                           std::filesystem::absolute(argv[0]).parent_path() /
                               "pathfinding_process_child.exe");
  std::cout << "Pathfinding failures: " << n << '\n';
  return n ? 1 : 0;
} catch (const std::exception &e) {
  std::cerr << e.what() << '\n';
  return 2;
}
