#include "PathfindingResults.h"
#include "ClientFolderPicker.h"
#include "PathfindingContext.h"
#include "PathfindingController.h"
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <imgui.h>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <territory/PathIO.h>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <shellapi.h>
#endif

std::optional<double> route_metric_ms(const pathfinding::Json &metrics,
                                      const char *name) {
  const auto found = metrics.find(name);
  if (found == metrics.end() || !found->is_number())
    return {};
  const auto ns = found->get<double>();
  if (!std::isfinite(ns) || ns < 0)
    return {};
  return ns / 1e6;
}
ResultTone route_result_tone(pathfinding::RouteStatus status) {
  using pathfinding::RouteStatus;
  switch (status) {
  case RouteStatus::Reached:
    return ResultTone::Success;
  case RouteStatus::Partial:
  case RouteStatus::UnknownLegacy:
  case RouteStatus::ResourceLimit:
  case RouteStatus::Cancelled:
  case RouteStatus::UnsupportedScope:
    return ResultTone::Warning;
  default:
    return ResultTone::Error;
  }
}
ResultFreshness route_result_freshness(bool running, bool has_results,
                                       std::uint64_t current_revision,
                                       std::uint64_t submitted_revision) {
  if (running)
    return ResultFreshness::Running;
  if (!has_results)
    return ResultFreshness::Idle;
  return current_revision == submitted_revision ? ResultFreshness::Current
                                                : ResultFreshness::Stale;
}
RouteSummary summarize_route(const pathfinding::RouteResult &result) {
  RouteSummary summary;
  summary.search_ms = route_metric_ms(result.metrics, "search_ns");
  if (result.metrics.contains("benchmark"))
    summary.search_ms = route_metric_ms(result.metrics.at("benchmark").at("search_ns"), "median_ns");
  if (summary.search_ms && result.metrics.contains("server_budget_ms") &&
      result.metrics.at("server_budget_ms").is_number()) {
    const auto budget = result.metrics.at("server_budget_ms").get<double>();
    if (std::isfinite(budget) && budget > 0)
      summary.budget_ratio = *summary.search_ms / budget;
  }
  summary.tone = route_result_tone(result.status);
  summary.points = result.final_path.size();
  if (!result.final_path.empty()) {
    summary.length_xyz = 0.;
    for (std::size_t i = 1; i < result.final_path.size(); ++i) {
      const auto a = result.final_path[i - 1], b = result.final_path[i];
      *summary.length_xyz += std::hypot(b.x - a.x, b.y - a.y, b.z - a.z);
    }
  }
  return summary;
}
std::string final_route_csv(const pathfinding::RouteResult &result) {
  if (result.final_path.empty())
    throw std::invalid_argument("No final route points to export");
  std::ostringstream output;
  output.imbue(std::locale::classic());
  output << std::setprecision(std::numeric_limits<double>::max_digits10)
         << "x,y,z\n";
  for (const auto &point : result.final_path) {
    pathfinding::validate_point(point);
    output << point.x << ',' << point.y << ',' << point.z << '\n';
  }
  return output.str();
}
void export_final_route_new(const pathfinding::RouteResult &result,
                            const std::filesystem::path &destination) {
  const auto bytes = final_route_csv(result);
#ifdef _WIN32
  const auto file = CreateFileW(destination.c_str(), GENERIC_WRITE, 0, nullptr,
                                CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE)
    throw std::system_error(GetLastError(), std::system_category(),
                            "Cannot create CSV; choose a NEW file");
  try {
    std::size_t offset = 0;
    while (offset < bytes.size()) {
      const auto amount = static_cast<DWORD>(
          std::min<std::size_t>(bytes.size() - offset, 1024 * 1024));
      DWORD written = 0;
      if (!WriteFile(file, bytes.data() + offset, amount, &written, nullptr))
        throw std::system_error(GetLastError(), std::system_category(),
                                "Cannot write route CSV");
      if (written != amount)
        throw std::runtime_error("Incomplete route CSV write");
      offset += written;
    }
    if (!FlushFileBuffers(file))
      throw std::system_error(GetLastError(), std::system_category(),
                              "Cannot flush route CSV");
  } catch (...) {
    CloseHandle(file);
    DeleteFileW(destination.c_str());
    throw;
  }
  CloseHandle(file);
#else
  auto *file = std::fopen(destination.c_str(), "wbx");
  if (!file)
    throw std::system_error(errno, std::generic_category(),
                            "Cannot create CSV; choose a NEW file");
  const bool written =
      std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
  const bool closed = std::fclose(file) == 0;
  if (!written || !closed) {
    std::error_code ignored;
    std::filesystem::remove(destination, ignored);
    throw std::runtime_error("Incomplete route CSV write");
  }
#endif
}

namespace {
ImVec4 result_color(ResultTone tone) {
  switch (tone) {
  case ResultTone::Success:
    return {0.35f, 0.9f, 0.45f, 1.f};
  case ResultTone::Warning:
    return {1.f, 0.75f, 0.3f, 1.f};
  case ResultTone::Error:
    return {1.f, 0.4f, 0.4f, 1.f};
  default:
    return {0.7f, 0.7f, 0.7f, 1.f};
  }
}
void open_result_path(const std::filesystem::path &path) {
#ifdef _WIN32
  if (reinterpret_cast<std::intptr_t>(ShellExecuteW(
          nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) <=
      32)
    throw std::runtime_error(
        "No application could open the selected report or folder");
#else
  (void)path;
  throw std::runtime_error("Opening reports requires Windows in this version");
#endif
}
template <typename F>
void result_action(PathfindingResultsState &state, F action) {
  state.action_message.clear();
  try {
    action();
    state.action_failed = false;
  } catch (const std::exception &error) {
    state.action_failed = true;
    state.action_message = error.what();
  }
}
void endpoint_details(
    const char *name,
    const std::optional<pathfinding::ResolvedEndpoint> &endpoint,
    const pathfinding::Endpoint &submitted) {
  ImGui::Text("%s requested XYZ: %.4f, %.4f, %.4f", name, submitted.requested.x,
              submitted.requested.y, submitted.requested.z);
  ImGui::Text("%s confirmed floor Z: %.4f", name, submitted.confirmed_z);
  if (!endpoint) {
    ImGui::TextDisabled("%s resolved endpoint: unavailable", name);
    return;
  }
  const auto &r = endpoint->resolved, &q = endpoint->requested;
  ImGui::Text("%s resolved XYZ: %.4f, %.4f, %.4f", name, r.x, r.y, r.z);
  ImGui::Text("Offset XY: %.4f | Z: %+.4f | confirmed floor Z: %+.4f",
              std::hypot(r.x - q.x, r.y - q.y), r.z - q.z,
              r.z - submitted.confirmed_z);
  ImGui::TextWrapped("Surface: %s | valid: %s", endpoint->surface_id.c_str(),
                     endpoint->valid ? "yes" : "no");
}
void result_details(const pathfinding::RouteResult &result,
                    const pathfinding::RouteCase &submitted, bool stale) {
  ImGui::SetNextWindowSize({660, 480}, ImGuiCond_Appearing);
  if (!ImGui::BeginPopup("Route details"))
    return;
  ImGui::Text("%s: %s%s", result.backend.c_str(),
              pathfinding::status_name(result.status), stale ? " (STALE)" : "");
  ImGui::BeginChild("Details scroll", {0, -ImGui::GetFrameHeightWithSpacing()},
                    true);
  if (stale)
    ImGui::TextWrapped(
        "These coordinates and measurements belong to the submitted "
        "case. Inputs have changed since that run.");
  ImGui::TextWrapped("%s", result.diagnostic.empty()
                               ? "No additional backend diagnostic."
                               : result.diagnostic.c_str());
  ImGui::Separator();
  const auto backend_case =
      pathfinding::case_for_backend(submitted, result.backend);
  endpoint_details("A", result.a, backend_case.a);
  endpoint_details("B", result.b, backend_case.b);
  ImGui::Text("Segments valid: %s", !result.segments_valid   ? "unavailable"
                                    : *result.segments_valid ? "yes"
                                                             : "no");
  ImGui::Text("Raw points: %zu | final: %zu | validated samples: %zu",
              result.raw.size(), result.final_path.size(),
              result.validated_samples.size());
  ImGui::Separator();
  ImGui::TextWrapped(
      "Single-query latency. Timer scopes can overlap and are not "
      "added together. n/a means no measurement was reported.");
  for (const auto &[key, label] :
       {std::pair{"search_ns", "Search"},
        {"direct_ns", "Direct check"},
        {"snap_ns", "Endpoint snap"},
        {"smoothing_ns", "Smoothing"},
        {"validation_ns", "Validation"},
        {"load_ns", "Backend loading"},
        {"prepare_ns", "World preparation (merge/save)"},
        {"merged_load_ns", "Merged mesh loading (same as backend load)"},
        {"setup_ns", "Backend setup"},
        {"startup_ns", "Java startup"},
        {"identity_ns", "Input identity check"},
        {"total_ns", "Backend query total"},
        {"backend_wall_ns", "Backend process wall"},
        {"worker_backend_wall_ns", "Worker backend wall"}}) {
    const auto ms = route_metric_ms(result.metrics, key);
    if (ms)
      ImGui::Text("%s: %.4f ms", label, *ms);
    else
      ImGui::TextDisabled("%s: n/a", label);
  }
  if (ImGui::TreeNode("All reported metrics")) {
    ImGui::TextWrapped("%s", result.metrics.dump(2).c_str());
    ImGui::TreePop();
  }
  if (ImGui::TreeNode("Result identity")) {
    ImGui::TextWrapped("Request: %s | case: %s", result.request_id.c_str(),
                       result.case_id.c_str());
    ImGui::TextWrapped("%s", result.identity.dump(2).c_str());
    ImGui::TreePop();
  }
  ImGui::EndChild();
  if (ImGui::Button("Close"))
    ImGui::CloseCurrentPopup();
  ImGui::EndPopup();
}
} // namespace

void draw_pathfinding_results(const PathfindingContext &context,
                              const PathfindingController &controller,
                              PathfindingResultsState &state) {
  const auto &results = controller.results();
  if (state.request_id != controller.request_id() ||
      state.summaries.size() != results.size()) {
    state.request_id = controller.request_id();
    state.action_message.clear();
    state.summaries.clear();
    for (const auto &result : results)
      state.summaries.push_back(summarize_route(result));
  }
  const auto freshness =
      route_result_freshness(controller.active(), !results.empty(),
                             context.revision, context.submitted_revision);
  const bool stale = freshness == ResultFreshness::Stale;
  ImGui::TextUnformatted("Results");
  ImGui::SameLine();
  if (freshness == ResultFreshness::Running)
    ImGui::TextColored({0.4f, 0.75f, 1.f, 1.f}, "RUNNING");
  else if (stale)
    ImGui::TextColored(result_color(ResultTone::Warning),
                       "STALE - inputs changed");
  else if (!controller.error().empty())
    ImGui::TextColored(result_color(ResultTone::Error), "JOB FAILED");
  else
    ImGui::TextDisabled("%s", results.empty() ? "No completed comparison"
                                              : "Current comparison");

  const auto &directory = controller.job_directory();
  if(controller.active() && !controller.benchmark_progress().is_null()) {
    const auto& p=controller.benchmark_progress();
    const auto done=p.at("completed").get<int>(), total=p.at("total").get<int>();
    const auto text=p.at("backend").get<std::string>()+" | "+p.at("phase").get<std::string>()+
      " "+std::to_string(done)+" / "+std::to_string(total);
    ImGui::ProgressBar(total?static_cast<float>(done)/total:1.f,{-1,0},text.c_str());
  }
  std::error_code ignored;
  const bool report_exists =
      !directory.empty() &&
      std::filesystem::is_regular_file(directory / "report.json", ignored);
  ImGui::BeginDisabled(!report_exists);
  if (ImGui::SmallButton("Open report"))
    result_action(state, [&] { open_result_path(directory / "report.json"); });
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(directory.empty());
  if (ImGui::SmallButton("Open folder"))
    result_action(state, [&] { open_result_path(directory); });
  ImGui::EndDisabled();
  if (!controller.error().empty() || !state.action_message.empty()) {
    ImGui::SameLine();
    if (ImGui::SmallButton("Messages..."))
      ImGui::OpenPopup("Result messages");
  }
  ImGui::SetNextWindowSize({580, 250}, ImGuiCond_Appearing);
  if (ImGui::BeginPopup("Result messages")) {
    ImGui::TextWrapped("%s", controller.error().c_str());
    ImGui::TextWrapped("%s", state.action_message.c_str());
    ImGui::TextWrapped("Folder: %s", territory::path_utf8(directory).c_str());
    ImGui::EndPopup();
  }

  const auto backends =
      !controller.request_id().empty()
          ? pathfinding::enabled_backends(controller.submitted_case())
      : context.dataset ? pathfinding::enabled_backends(pathfinding::RouteCase{
                              {},
                              {},
                              {},
                              {},
                              context.dataset->l2j_identity(),
                              context.dataset->nav_identity(),8,32,{}, {},context.dataset->nav_regions()})
                        : std::vector<std::string>{};
  if (!backends.empty() &&
      ImGui::BeginTable("Backend results", static_cast<int>(backends.size()),
                        ImGuiTableFlags_BordersInnerV |
                            ImGuiTableFlags_SizingStretchSame)) {
    for (const auto &backend : backends) {
      const auto *label = backend == "l2j" ? "L2J" : "Navmesh";
      ImGui::TableNextColumn();
      ImGui::PushID(backend.c_str());
      const auto found =
          std::find_if(results.begin(), results.end(), [&](const auto &result) {
            return result.backend == backend;
          });
      if (found == results.end()) {
        ImGui::Text("%s", label);
        ImGui::TextDisabled("%s", controller.active() ? "Pending result..."
                                                      : "No result");
        ImGui::TextDisabled("Search: n/a");
        ImGui::PopID();
        continue;
      }
      const auto &result = *found;
      const bool benchmark=result.metrics.contains("benchmark");
      const auto &summary =
          state.summaries[static_cast<std::size_t>(found - results.begin())];
      ImGui::TextColored(
          result_color(stale ? ResultTone::Neutral : summary.tone), "%s: %s",
          label, pathfinding::status_name(result.status));
      char search[80];
      if (summary.search_ms)
        std::snprintf(search, sizeof search, benchmark?"Search median: %.4f ms":"Search: %.4f ms",
                      *summary.search_ms);
      else
        std::snprintf(search, sizeof search, "Search: n/a");
      const float size = ImGui::GetFontSize() * 1.2f;
      ImGui::GetWindowDrawList()->AddText(
          ImGui::GetFont(), size, ImGui::GetCursorScreenPos(),
          ImGui::GetColorU32(ImGuiCol_Text), search);
      ImGui::Dummy({0, size});
      if(benchmark) {
        const auto& b=result.metrics.at("benchmark");
        ImGui::Text("Warmup %d | measured %d",b.at("warmup").get<int>(),b.at("iterations").get<int>());
        const auto p95=route_metric_ms(b.at("search_ns"),"p95_ns"),p99=route_metric_ms(b.at("search_ns"),"p99_ns");
        if(p95 && p99)ImGui::Text("p95 %.4f | p99 %.4f ms",*p95,*p99);
        else ImGui::TextDisabled("Search: n/a (e.g. direct L2J route)");
        const auto total=route_metric_ms(b.at("total_ns"),"median_ns");
        if(total)ImGui::Text("Query total median: %.4f ms",*total);
        ImGui::TextWrapped("Outcomes: %s",b.at("status_counts").dump().c_str());
        if(!b.at("limit_counts").empty())ImGui::TextWrapped("Limits: %s",b.at("limit_counts").dump().c_str());
        ImGui::TextDisabled("Status: last measurement");
        if(result.backend=="l2j")ImGui::TextDisabled("Route: separate evidence replay");
      }
      if (summary.budget_ratio)
        ImGui::TextColored(
            result_color(*summary.budget_ratio > 1 ? ResultTone::Warning
                                                   : ResultTone::Success),
            "Server ref: %d ms | %.2fx%s",
            controller.submitted_case().search.server_budget_ms,
            *summary.budget_ratio, *summary.budget_ratio > 1 ? " OVER" : "");
      if (result.metrics.contains("limit_reason") &&
          result.metrics.at("limit_reason").is_string() &&
          !result.metrics.at("limit_reason").get<std::string>().empty())
        ImGui::TextWrapped(
            "Limit: %s",
            result.metrics.at("limit_reason").get<std::string>().c_str());
      if (summary.length_xyz)
        ImGui::Text("%zu pts | %.1f game units (XYZ)", summary.points,
                    *summary.length_xyz);
      else
        ImGui::TextDisabled("0 points | length n/a");
      if(result.metrics.contains("visited_regions"))
        ImGui::TextWrapped("Regions: %s",result.metrics.at("visited_regions").dump().c_str());
      if(result.status==pathfinding::RouteStatus::UnsupportedScope)
        ImGui::TextWrapped("Skipped: endpoints exceed this backend's loaded scope.");
      if (result.a && result.b)
        ImGui::Text("Z offset A %+.1f | B %+.1f",
                    result.a->resolved.z - result.a->requested.z,
                    result.b->resolved.z - result.b->requested.z);
      else
        ImGui::TextDisabled("Endpoint offsets: see Details");
      ImGui::BeginDisabled(result.final_path.empty());
      if (ImGui::SmallButton("Export CSV..."))
        result_action(state, [&] {
          if (const auto file = choose_file(
                  L"Export final game XYZ to a NEW CSV file", true, L"csv")) {
            export_final_route_new(result, *file);
            state.action_message =
                "Saved " + result.backend +
                " final route: " + territory::path_utf8(*file);
          }
        });
      ImGui::EndDisabled();
      ImGui::SameLine();
      if (ImGui::SmallButton("Details..."))
        ImGui::OpenPopup("Route details");
      result_details(result, controller.submitted_case(), stale);
      ImGui::PopID();
    }
    ImGui::EndTable();
  }
  if (!state.action_message.empty())
    ImGui::TextColored(result_color(state.action_failed ? ResultTone::Error
                                                        : ResultTone::Success),
                       "%s - see Messages",
                       state.action_failed ? "Action failed" : "Export saved");
  else
    ImGui::TextDisabled("Search only; loading/startup/wall times in Details.");
}
