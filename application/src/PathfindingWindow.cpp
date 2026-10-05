#include "PathfindingWindow.h"
#include "ClientFolderPicker.h"
#include "PathfindingOverlay.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <imgui.h>
#include <territory/PathIO.h>

namespace {
struct Disabled {
  explicit Disabled(bool value) { ImGui::BeginDisabled(value); }
  ~Disabled() { ImGui::EndDisabled(); }
};
std::string label(const std::filesystem::path &p) {
  auto s = p.u8string();
  return {s.begin(), s.end()};
}
void file_control(const char *title, const wchar_t *dialog_title,
                  std::filesystem::path &path, bool &changed,
                  bool allow_clear = true) {
  ImGui::PushID(title);
  if (ImGui::Button(title))
    if (auto p = choose_file(dialog_title)) {
      path = *p;
      changed = true;
    }
  if (allow_clear && !path.empty()) {
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear")) {
      path.clear();
      changed = true;
    }
  }
  ImGui::SameLine();
  ImGui::TextWrapped("%s", path.empty() ? "Not selected" : label(path).c_str());
  ImGui::PopID();
}
void point_control(const char *name,
                   const std::optional<pathfinding::Endpoint> &e) {
  if (!e) {
    ImGui::Text("%s: not set", name);
    return;
  }
  ImGui::Text("%s: %.2f, %.2f, %.2f", name, e->requested.x, e->requested.y,
              e->requested.z);
  ImGui::SameLine();
  ImGui::PushID(name);
  if (ImGui::SmallButton("Copy XYZ")) {
    char text[192];
    std::snprintf(text, sizeof text, "%.3f, %.3f, %.3f", e->requested.x,
                  e->requested.y, e->requested.z);
    ImGui::SetClipboardText(text);
  }
  ImGui::PopID();
}
ImVec2 screen(const PathfindingContext &c, pathfinding::WorldPoint p,
              glm::ivec2 viewport) {
  const auto s = world_to_screen_xy(c.view, {p.x, p.y}, viewport);
  return {float(s.x), float(s.y)};
}
void draw_overlay(const PathfindingContext &c,
                  const std::vector<pathfinding::RouteResult> &results,
                  const std::vector<RouteSegment> &nav_lines,
                  const std::vector<pathfinding::OverviewCell> &geo_cells,
                  glm::ivec2 size) {
  if (!c.flight || size.x <= 0 || size.y <= 0)
    return;
  auto *draw = ImGui::GetBackgroundDrawList();
  const auto lo = screen_to_world_xy(c.view, {0, 0}, size),
             hi = screen_to_world_xy(c.view, {size.x, size.y}, size);
  const auto line = [&](pathfinding::WorldPoint a, pathfinding::WorldPoint b,
                        unsigned color, float width) {
    if (std::max(a.x, b.x) < lo.x || std::min(a.x, b.x) > hi.x ||
        std::max(a.y, b.y) < lo.y || std::min(a.y, b.y) > hi.y)
      return;
    draw->AddLine(screen(c, a, size), screen(c, b, size), color, width);
  };
  if (c.show_cells && c.dataset) {
    for (const auto &cell : geo_cells) {
      const auto a = cell.minimum;
      const auto w = cell.width;
      const unsigned color = cell.mixed || (cell.nswe != 0 && cell.nswe != 15)
                                 ? 0x9030c8ffu
                             : cell.nswe ? 0x9040d060u
                                         : 0xa04040efu;
      auto p0 = screen(c, a, size),
           p1 = screen(c, {a.x + w, a.y + w, a.z}, size);
      p0.x = std::max(0.f, p0.x);
      p0.y = std::max(0.f, p0.y);
      p1.x = std::min(float(size.x), p1.x);
      p1.y = std::min(float(size.y), p1.y);
      if (p0.x >= p1.x || p0.y >= p1.y)
        continue;
      draw->AddRectFilled(p0, p1, (color & 0x00ffffffu) | 0x60000000u);
      if (!cell.detailed || w * size.x / c.view.width < 12)
        continue;
      const pathfinding::WorldPoint p[4] = {{a.x, a.y, a.z},
                                            {a.x + w, a.y, a.z},
                                            {a.x + w, a.y + w, a.z},
                                            {a.x, a.y + w, a.z}};
      for (int i = 0; i < 4; ++i)
        line(p[i], p[(i + 1) % 4], color, 1);
      const pathfinding::WorldPoint mid{a.x + w / 2, a.y + w / 2, a.z};
      // Standard L2J bits: E=1, W=2, S=4, N=8.
      for (int i = 0; i < 4; ++i)
        if (cell.nswe & (1u << i)) {
          auto tip = mid;
          if (i == 0)
            tip.x += w * .35;
          else if (i == 1)
            tip.x -= w * .35;
          else if (i == 2)
            tip.y += w * .35;
          else
            tip.y -= w * .35;
          line(mid, tip, color, 1);
        }
    }
  }
  if (c.show_polygons)
    for (const auto &s : nav_lines)
      line(s.a, s.b, s.color, 1);
  const auto overlay = route_overlay(c, results);
  if (c.show_raw)
    for (const auto &r : results)
      if (r.backend == "navmesh")
        for (const auto &poly : r.corridor) {
          for (std::size_t i = 0; i < poly.size(); ++i) {
            const auto &a = poly[i], &b = poly[(i + 1) % poly.size()];
            if (c.show_full_route || (std::min(a.z, b.z) <= c.view.z_max &&
                                      std::max(a.z, b.z) >= c.view.z_min))
              line(a, b,
                   c.revision == c.submitted_revision ? 0x90ffdc50u
                                                      : 0x90909090u,
                   1);
          }
        }
  std::size_t index = 0;
  for (const auto &s : overlay.segments) {
    line(s.a, s.b, s.color, s.raw ? 1.f : 3.f);
    if (!s.raw && ++index % 12 == 0) {
      const auto a = screen(c, s.a, size), b = screen(c, s.b, size);
      glm::vec2 d{b.x - a.x, b.y - a.y};
      if (glm::length(d) > 1) {
        d = glm::normalize(d);
        const glm::vec2 p{-d.y, d.x};
        draw->AddTriangleFilled(
            b, {b.x - d.x * 8 + p.x * 4, b.y - d.y * 8 + p.y * 4},
            {b.x - d.x * 8 - p.x * 4, b.y - d.y * 8 - p.y * 4}, s.color);
      }
    }
  }
  const auto marker = [&](const std::optional<pathfinding::Endpoint> &e,
                          const char *name, unsigned color) {
    if (!e)
      return;
    const auto p = screen(c, e->requested, size);
    draw->AddCircleFilled(p, 6, color);
    draw->AddText({p.x + 9, p.y - 8}, color, name);
  };
  marker(c.a, "A", 0xff50ff50u);
  marker(c.b, "B", 0xff5050ffu);
  if (c.hover) {
    const auto p = screen(c, {c.hover->x, c.hover->y, 0}, size);
    draw->AddLine({p.x - 8, p.y}, {p.x + 8, p.y}, 0xffffffffu, 1);
    draw->AddLine({p.x, p.y - 8}, {p.x, p.y + 8}, 0xffffffffu, 1);
    char text[120];
    std::snprintf(text, sizeof text, "XY %.2f, %.2f | click to choose floor",
                  c.hover->x, c.hover->y);
    draw->AddText({p.x + 12, p.y + 12}, 0xffffffffu, text);
  }
  if (overlay.outside)
    draw->AddText({float(size.x) - 300, 16}, 0xffeeeeeeu,
                  "Route extends outside the Z slice");
}
} // namespace

void PathfindingWindow::update_navigation_overlay(glm::ivec2 size) {
  const auto &c = context;
  if (!c.dataset || !c.flight || size.x <= 0 || size.y <= 0) {
    overlay_dataset.reset();
    nav_lines.clear();
    geo_cells.clear();
    return;
  }
  const bool data_changed = overlay_dataset != c.dataset;
  const bool slice_changed =
      overlay_view.z_min != c.view.z_min || overlay_view.z_max != c.view.z_max;
  const bool view_changed = overlay_view.center != c.view.center ||
                            overlay_view.width != c.view.width ||
                            overlay_size != size;
  if (c.show_polygons && (data_changed || slice_changed || !cached_polygons))
    nav_lines = navigation_polygon_lines(polygons, c.view);
  const auto next_cell_width = navigation_cell_width(c.view, size);
  // Cache the whole region at coarse scales (at most 65,536 bins). Panning
  // then only clips existing bins, rather than rescanning millions of layers.
  const bool full_region = next_cell_width >= 128;
  if (c.show_cells &&
      (data_changed || slice_changed || next_cell_width != cell_width ||
       (!full_region && view_changed) || !cached_cells)) {
    auto lo = screen_to_world_xy(c.view, {0, 0}, size),
         hi = screen_to_world_xy(c.view, {size.x, size.y}, size);
    if (full_region) {
      const auto origin = c.dataset->origin();
      lo = {origin.x, origin.y};
      hi = lo + glm::dvec2{32768, 32768};
    }
    cell_width = next_cell_width;
    geo_cells = c.dataset->l2j_overview({lo.x, lo.y, hi.x, hi.y}, c.view.z_min,
                                        c.view.z_max, cell_width);
  }
  overlay_dataset = c.dataset;
  overlay_view = c.view;
  overlay_size = size;
  cached_cells = c.show_cells;
  cached_polygons = c.show_polygons;
}

void PathfindingWindow::load() {
  context.clear_points();
  context.dataset.reset();
  polygons.clear();
  context.error.clear();
  message.clear();
  loader.request({map.data(), l2j, navmesh, loaded_case, nav_regions});
}
pathfinding::RouteCase PathfindingWindow::route() const {
  if (!context.dataset || !context.a || !context.b)
    throw std::runtime_error("Load data and choose both A and B");
  pathfinding::RouteCase r{case_id,
                           context.dataset->map(),
                           *context.a,
                           *context.b,
                           context.dataset->l2j_identity(),
                           context.dataset->nav_identity()};
  r.nav_regions=context.dataset->nav_regions();
  if (loaded_case) {
    r.snap_horizontal = loaded_case->snap_horizontal;
    r.snap_vertical = loaded_case->snap_vertical;
  }
  r.expected_backends = expected_backends;
  r.search = search;
  pathfinding::validate_case(r);
  return r;
}
void PathfindingWindow::frame(PathfindingController &controller,
                              rendering::Camera &camera,
                              std::string_view current_map) {
  auto &c = context;
  const auto initial_map =
      default_navigation_map(map.data(), current_map,
                             !l2j.empty() || !navmesh.empty() || !nav_regions.empty() || c.dataset ||
                                 loaded_case || loader.active());
  std::snprintf(map.data(), map.size(), "%s", initial_map.c_str());
  controller.poll();
  if (auto completion = loader.poll()) {
    c.dataset = std::move(completion->dataset);
    c.error = completion->error;
    if (c.dataset) {
      polygons = c.dataset->nav_polygons(-10000000, 10000000);
      if (loaded_case) {
        c.a = loaded_case->a;
        c.b = loaded_case->b;
        case_id = loaded_case->id;
        c.view.center = {c.a->requested.x, c.a->requested.y};
      } else {
        case_id =
            "mouse-" +
            std::to_string(
                std::chrono::system_clock::now().time_since_epoch().count());
        const auto bounds=c.dataset->game_bounds();
        c.view.center={(bounds.x+bounds.z)/2,(bounds.y+bounds.w)/2};
      }
      ++c.revision;
    }
  }
  ImGui::SetNextWindowSize({520, 640}, ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Pathfinding Lab")) {
    draw_pathfinding_results(c, controller, results_state);
    ImGui::Separator();
    ImGui::BeginChild("Lab controls", {0, 0}, false);
    try {
      ImGui::TextWrapped("Offline route comparison. Navmesh region set; no "
                         "server changes.");
      bool top = c.flight.has_value();
      if (ImGui::Checkbox("Top view", &top))
        c.toggle_top(camera);
      ImGui::SameLine();
      if (ImGui::Button("Center on data") && c.dataset) {
        const auto bounds=c.dataset->game_bounds();
        const auto display=ImGui::GetIO().DisplaySize;
        c.view.width=std::clamp(1.05*std::max(bounds.z-bounds.x,(bounds.w-bounds.y)*display.x/std::max(display.y,1.f)),128.0,max_top_view_width);
        c.focus({float((bounds.x+bounds.z)/2),float((bounds.y+bounds.w)/2),32768},camera);
      }
      ImGui::TextUnformatted(
          "Wheel: zoom | Middle drag: pan | Escape: cancel picking");
      {
        const Disabled disabled{controller.active()};
        ImGui::SetNextItemWidth(110);
        bool changed = ImGui::InputText("Map (dd_dd)", map.data(), map.size());
        ImGui::SameLine();
        {
          const auto current = default_navigation_map("", current_map, false);
          const Disabled disabled_current{current.empty()};
          if (ImGui::SmallButton("Current") && current != map.data()) {
            std::snprintf(map.data(), map.size(), "%s", current.c_str());
            l2j.clear();
            navmesh.clear();
            nav_regions.clear();
            changed = true;
          }
          if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Use the map under the camera. Changing map "
                              "clears the old navigation files and points.");
        }
        file_control("L2J file", L"Select a generated L2J region", l2j,
                     changed);
        if(ImGui::Button("Add Navmesh..."))
          if(auto file=choose_file(L"Add a generated Navmesh region",false,L"navmesh")) {
            if(!navmesh.empty()){nav_regions.push_back(navmesh);navmesh.clear();}
            if(std::find(nav_regions.begin(),nav_regions.end(),*file)==nav_regions.end())nav_regions.push_back(*file);
            changed=true;
          }
        if(!navmesh.empty())file_control("Legacy Navmesh",L"Select a legacy Navmesh region",navmesh,changed);
        for(std::size_t i=0;i<nav_regions.size();) {
          ImGui::PushID(static_cast<int>(i));
          if(ImGui::SmallButton("Remove")) {nav_regions.erase(nav_regions.begin()+i);changed=true;ImGui::PopID();continue;}
          ImGui::SameLine();ImGui::TextWrapped("%s",label(nav_regions[i]).c_str());
          ImGui::PopID();++i;
        }
        ImGui::TextDisabled("Navmesh regions are read automatically from adjacent metadata.");
        ImGui::TextWrapped(
            "Load either format or both. Search runs only the loaded formats.");
        if (changed) {
          loader.invalidate();
          loaded_case.reset();
          expected_backends.clear();
          c.clear_points();
          c.dataset.reset();
          polygons.clear();
        }
        if (ImGui::Button("Load navigation data")) {
          loaded_case.reset();
          expected_backends.clear();
          load();
        }
        ImGui::SameLine();
        if (ImGui::Button("Load case..."))
          if (auto file = choose_file(L"Load a saved route case")) {
            loaded_case = pathfinding::read_case(*file);
            search = loaded_case->search;
            expected_backends = loaded_case->expected_backends;
            l2j = loaded_case->l2j.path;
            navmesh = loaded_case->navmesh.path;
            nav_regions.clear();for(const auto& r:loaded_case->nav_regions)nav_regions.push_back(r.mesh.path);
            std::snprintf(map.data(), map.size(), "%s",
                          loaded_case->map.c_str());
            load();
          }
        if (loaded_case && !c.dataset && !loader.active() && !c.error.empty()) {
          ImGui::TextWrapped(
              "Saved inputs could not be verified. Reload keeps old hashes. "
              "Adopt explicitly starts a NEW comparison using current files.");
          if (ImGui::Button("Adopt current files explicitly")) {
            // Hashes are read by the background loader, never on the render
            // thread.
            auto points = *loaded_case;
            loaded_case.reset();
            load();
            loaded_case = std::move(points);
          }
        }
        ImGui::TextDisabled("Backend: %s", profile_path.empty()
                                               ? "not configured"
                                               : "ready (details below)");
        if (ImGui::TreeNode("Backend settings (advanced)")) {
          bool profile_changed = false;
          auto candidate_profile = profile_path;
          file_control("Backend profile",
                       L"Select prepared Java backend profile JSON",
                       candidate_profile, profile_changed, false);
          if (profile_changed) {
            try {
              c.select_profile(candidate_profile, profile, profile_path);
            } catch (const std::exception &e) {
              c.error = e.what();
            }
          }
          ImGui::Text("Saved backend baselines: %zu", expected_backends.size());
          if (expected_backends.empty())
            ImGui::TextWrapped("Save case after Find path to record the tested "
                               "backend identities.");
          else if (ImGui::Button(
                       "Accept current backends for a NEW comparison")) {
            expected_backends.clear();
            ++c.revision;
          }
          ImGui::TreePop();
        }
      }
      if (loader.active())
        ImGui::TextUnformatted("Loading navigation data in background...");
      if (c.dataset) {
        ImGui::Text("Data: %s | load %.2f ms", c.dataset->map().c_str(),
                    double(c.dataset->load_ns()) / 1e6);
        for(const auto& r:c.dataset->nav_regions())ImGui::Text("Navmesh region: %s",r.map.c_str());
        if (ImGui::TreeNode("Input identities")) {
          if(!c.dataset->l2j_identity().path.empty())
            ImGui::TextWrapped("L2J SHA-256: %s",c.dataset->l2j_identity().sha256.c_str());
          if(!c.dataset->nav_identity().path.empty())
            ImGui::TextWrapped("Navmesh SHA-256: %s",c.dataset->nav_identity().sha256.c_str());
          for(const auto& region:c.dataset->nav_regions()) {
            ImGui::TextWrapped("%s Navmesh SHA-256: %s",region.map.c_str(),region.mesh.sha256.c_str());
            ImGui::TextWrapped("%s Metadata SHA-256: %s",region.map.c_str(),region.metadata.sha256.c_str());
          }
          ImGui::TextUnformatted(
              "Generation settings belong to each source's generation report.");
          ImGui::TreePop();
        }
      }
      if (c.dataset && !c.dataset->contains(c.view.center.x, c.view.center.y))
        ImGui::TextUnformatted(
            "View center is OUTSIDE the loaded navigation region.");
      if (c.flight) {
        double z[2] = {c.view.z_min, c.view.z_max};
        if (ImGui::InputDouble("Z min", &z[0], 16, 128, "%.1f") &&
            std::isfinite(z[0]) && z[0] < z[1])
          c.view.z_min = std::clamp(z[0], -10000000., 10000000.);
        if (ImGui::InputDouble("Z max", &z[1], 16, 128, "%.1f") &&
            std::isfinite(z[1]) && z[1] > z[0])
          c.view.z_max = std::clamp(z[1], -10000000., 10000000.);
      }
      {
        const Disabled disabled{!c.dataset || !c.flight || loader.active()};
        if (ImGui::Button("Set A")) {
          c.picking = PickMode::A;
          c.candidates.clear();
          c.press.reset();
        }
        ImGui::SameLine();
        if (ImGui::Button("Set B")) {
          c.picking = PickMode::B;
          c.candidates.clear();
          c.press.reset();
        }
        ImGui::SameLine();
        if (ImGui::Button("Swap"))
          c.swap_points();
        ImGui::SameLine();
        if (ImGui::Button("Clear"))
          c.clear_points();
      }
      if (c.picking != PickMode::None)
        ImGui::Text("Click the scene to set %s; choose a floor if prompted.",
                    c.picking == PickMode::A ? "A" : "B");
      point_control("A", c.a);
      point_control("B", c.b);
      if(c.dataset)for(const auto& [name,point]:{std::pair{"A",c.a},{"B",c.b}})
        if(point)if(auto region=c.dataset->region_at(point->requested.x,point->requested.y))ImGui::TextDisabled("%s region: %s",name,region->c_str());
      for (const auto &[name, point] : {std::pair{"A", c.a}, {"B", c.b}})
        if (point && point->backend_z.size() > 1)
          ImGui::TextDisabled("%s floor Z: L2J %.2f | Navmesh %.2f", name,
                              point->backend_z.at("l2j"),
                              point->backend_z.at("navmesh"));
      if (!c.candidates.empty()) {
        ImGui::TextUnformatted(
            "Choose the actual floor (no automatic topmost choice):");
        ImGui::BeginChild("floors", {0, 100}, true);
        for (std::size_t i = 0; i < c.candidates.size(); ++i) {
          const auto &p = c.candidates[i];
          char text[256];
          std::snprintf(text, sizeof text, "Z %.2f | %s | %s##%zu",
                        p.resolved.z, p.surface_id.c_str(),
                        p.valid ? "walkable" : "NSWE=0", i);
          if (ImGui::Selectable(text)) {
            c.choose_floor(i);
            break;
          }
        }
        ImGui::EndChild();
      }
      {
        const Disabled disabled{controller.active()};
        ImGui::Separator();
        ImGui::TextUnformatted("Search limits (offline experiment)");
        bool changed = ImGui::InputInt("Server reference (ms)",
                                       &search.server_budget_ms, 10, 100);
        changed |= ImGui::InputInt("Search timeout (ms)", &search.timeout_ms,
                                   100, 1000);
        changed |= ImGui::InputInt("Nav expansion limit", &search.nav_max_nodes,
                                   1000, 10000);
        changed |= ImGui::InputInt("L2J window (cells)",
                                   &search.l2j_max_window_cells, 32, 256);
        if (changed) {
          search.server_budget_ms =
              std::clamp(search.server_budget_ms, 1, 30000);
          search.timeout_ms = std::clamp(search.timeout_ms, 1, 30000);
          search.nav_max_nodes = std::clamp(search.nav_max_nodes, 1, 1000000);
          search.l2j_max_window_cells =
              std::clamp(search.l2j_max_window_cells, 64, 2048) / 32 * 32;
          ++c.revision;
        }
        ImGui::TextWrapped(
            "Server reference is a comparison, NOT a stop. L2J window: %d "
            "world units per side, not route length. One square only. Process "
            "safety limit: 60 s per backend.",
            search.l2j_max_window_cells * 16);
        if (!profile.legacy_experimental_limits && !l2j.empty())
          ImGui::TextColored(
              {1.f, .7f, .3f, 1.f},
              "Old L2J backend: experiment controls unavailable.");
      }
      {
        const Disabled disabled{controller.active()};
        ImGui::Separator();
        ImGui::TextUnformatted("JVM benchmark (same A -> B)");
        bool changed=ImGui::InputInt("Warmup searches",&benchmark.warmup,100,1000);
        changed|=ImGui::InputInt("Measured searches",&benchmark.iterations,10,100);
        if(changed) {
          benchmark.warmup=std::clamp(benchmark.warmup,0,10000);
          benchmark.iterations=std::clamp(benchmark.iterations,1,1000);
          ++c.revision;
        }
        ImGui::TextWrapped("One data load and one JVM per backend. Warmup is excluded. "
          "Repeats this route, not a mixed server workload. Series limit: 10 min/backend; Cancel stops the job.");
      }
      {
        const Disabled disabled{controller.active() || !c.dataset || !c.a ||
                                !c.b};
        if (ImGui::Button("Find path")) {
          if (controller.start(route(), profile))
            c.submitted_revision = c.revision;
        }
        ImGui::SameLine();
        if(ImGui::Button("Warmup + measure")) {
          if(controller.start(route(),profile,benchmark)) c.submitted_revision=c.revision;
        }
      }
      {
        const Disabled disabled{controller.active() || !c.dataset || !c.a || !c.b};
        if (ImGui::Button("Save case..."))
          if (auto file = choose_file(L"Save case to a NEW JSON file", true)) {
            auto saved = route();
            if (c.revision == c.submitted_revision)
              for (const auto &result : controller.results())
                if (result.identity.contains("backend_manifest"))
                  saved.expected_backends[result.backend] =
                      result.identity.at("backend_manifest")
                          .at("sha256")
                          .get<std::string>();
            pathfinding::write_case_new(saved, *file);
            expected_backends = std::move(saved.expected_backends);
            message = "Saved: " + label(*file);
          }
      }
      ImGui::SameLine();
      {
        const Disabled disabled{!controller.active()};
        if (ImGui::Button("Cancel job"))
          controller.cancel();
      }
      ImGui::Checkbox("Raw route / corridor", &c.show_raw);
      ImGui::SameLine();
      ImGui::Checkbox("Final route", &c.show_final);
      ImGui::Checkbox("Validated samples", &c.show_validated);
      ImGui::SameLine();
      ImGui::Checkbox("Full route outside slice", &c.show_full_route);
      ImGui::Checkbox("L2J coverage + NSWE", &c.show_cells);
      ImGui::SameLine();
      ImGui::Checkbox("Nav polygons", &c.show_polygons);
      if (c.show_cells) {
        ImGui::TextWrapped(
            "L2J: green = open samples; red = blocked; amber = "
            "restricted/mixed. "
            "Coarse bins summarize occupied cells, not exact boundaries. "
            "All layers in Z slice are projected.");
        ImGui::Text("Display bin: %u units. Exact cell arrows appear on zoom.",
                    cell_width);
      }
      ImGui::TextUnformatted(
          "Orange: L2J | Cyan: Navmesh | Grey: stale | Thin: raw");
      ImGui::TextWrapped(
          "Find path: cold single query. Warmup + measure: repeated-route latency, "
          "NOT server throughput. Startup/loading are separate from search. "
          "A fixed warmup count does not guarantee JIT stability.");
      if (!controller.job_directory().empty())
        ImGui::TextWrapped(
            "Report: %s",
            label(controller.job_directory() / "report.json").c_str());
    } catch (const std::exception &e) {
      c.error = e.what();
    }
    if (!message.empty())
      ImGui::TextWrapped("%s", message.c_str());
    if (!c.error.empty())
      ImGui::TextWrapped("%s", c.error.c_str());
    if (!controller.error().empty())
      ImGui::TextWrapped("%s", controller.error().c_str());
    ImGui::EndChild();
  }
  ImGui::End();
  auto &io = ImGui::GetIO();
  const PathfindingInput input{{io.MousePos.x, io.MousePos.y},
                               {io.MouseDelta.x, io.MouseDelta.y},
                               {int(io.DisplaySize.x), int(io.DisplaySize.y)},
                               ImGui::IsMouseClicked(0),
                               ImGui::IsMouseReleased(0),
                               io.MouseDown[0],
                               io.MouseDown[1],
                               io.MouseDown[2],
                               io.WantCaptureMouse,
                               ImGui::IsKeyPressed(ImGuiKey_Escape),
                               io.MouseWheel};
  try {
    update_pathfinding_input(c, input);
    if (c.flight)
      camera.set_top_view(c.view.center, float(c.view.width),
                          float(c.view.z_min), float(c.view.z_max));
    update_navigation_overlay(input.viewport);
    draw_overlay(c, controller.results(), nav_lines, geo_cells, input.viewport);
  } catch (const std::exception &e) {
    c.error = e.what();
  }
}
