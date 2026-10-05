#include "GeodataBuildJob.h"
#include "LoadingSystem.h"
#include "NavmeshRegionGeometry.h"

#include "BuildIdentity.h"
#include <chrono>
#include <fstream>
#include <geodata/Builder.h>
#include <geodata/Exporter.h>
#include <iostream>
#include <navmesh/Navmesh.h>
#include <territory/PathIO.h>
#include <utils/Log.h>

namespace {
class WorkerLog {
public:
  explicit WorkerLog(const std::filesystem::path &path) : file(path) {
    if (!file)
      throw std::runtime_error("Cannot create geodata worker.log");
    output = std::cout.rdbuf(file.rdbuf());
    error = std::cerr.rdbuf(file.rdbuf());
    utils::Log::colored = false;
    utils::Log::level = utils::LOG_INFO;
  }
  ~WorkerLog() {
    std::cout.flush();
    std::cerr.flush();
    std::cout.rdbuf(output);
    std::cerr.rdbuf(error);
  }

private:
  std::ofstream file;
  std::streambuf *output{}, *error{};
};
} // namespace

int run_geodata_job_file(const std::filesystem::path &directory) {
  try {
    if (!directory.is_absolute())
      throw std::invalid_argument("Geodata job directory must be absolute");
    const auto job = geodata_job_from_json(
        territory::read_json(directory / "request.json"), directory);
    // Claim before opening logs or replacing status. A repeated invocation
    // must leave the first worker's outputs and progress alone.
    territory::write_json_atomic(directory / "started.json",
                                 {{"job_id", job.id}}, false);
    WorkerLog log(directory / "worker.log");
    territory::Status status;
    status.job_id = job.id;
    status.map_count = job.maps.size();
    std::string format;
    const auto publish = [&] {
      auto json = territory::to_json(status);
      json["format"] = format;
      territory::write_json_atomic(directory / "status.json", json, true);
    };
    const auto cancelled = [&] {
      return territory::cancellation_requested(directory);
    };
    int code = 0;
    std::vector<std::string> files;
    auto navmesh_reports = territory::Json::array();
    try {
      publish();
      for (const auto &name : job.maps) {
        territory::check_cancel(cancelled);
        status.map = name;
        format.clear();
        status.tiles_done = status.tiles_total = 0;
        status.phase = territory::Phase::Loading;
        publish();
        if (!std::filesystem::is_regular_file(job.client / "Maps" /
                                              (name + ".unr")))
          throw std::runtime_error("Map input is missing: " + name);
        GeodataContext context;
        // Same full collision geometry path as Application::build; no viewer
        // residency, visual scene or OpenGL context is used by this worker.
        if (job.l2j || !job.navmesh_neighbor_context) {
          LoadingSystem load{context, nullptr, job.client, {name}};
        }
        territory::check_cancel(cancelled);
        if ((job.l2j || !job.navmesh_neighbor_context) && (context.maps.size() != 1 ||
            context.maps.front().vertices().empty() ||
            context.maps.front().indices().empty()))
          throw std::runtime_error("Map has no complete collision geometry: " +
                                   name);
        if (job.l2j) {
          format = "l2j";
          status.phase = territory::Phase::Rendering;
          publish();
          geodata::Builder builder;
          const auto &buffer =
              builder.build(context.maps.front(), job.settings);
          territory::check_cancel(cancelled);
          status.phase = territory::Phase::Saving;
          publish();
          geodata::Exporter exporter{directory};
          exporter.export_geodata(buffer, name, job.client_dat);
          files.push_back(name + ".l2j");
          if (job.client_dat)
            files.push_back(name + "_conv.dat");
        }
        if (job.navmesh) {
          territory::check_cancel(cancelled);
          format = "navmesh";
          status.phase = territory::Phase::Rendering;
          publish();
          const auto started = std::chrono::steady_clock::now();
          std::optional<NavmeshRegionGeometry> region;
          if (job.navmesh_neighbor_context) {
            // L2J has already consumed its unchanged own-map input.
            context.maps.clear();
            status.phase = territory::Phase::Loading;
            publish();
            region.emplace(load_navmesh_region_geometry(job.client, name, job.navmesh_settings, cancelled,
              [&](const std::string& source, unreal::ArchiveReadObserver observer) {
                GeodataContext loaded;
                LoadingSystem loader{loaded, nullptr, job.client, {source}, std::move(observer)};
                if (loaded.maps.size()!=1) throw std::runtime_error("Missing collision geometry: "+source);
                return std::move(loaded.maps.front());
              }));
            region->metadata.generator=build_identity;
            status.warnings += region->metadata.missing_neighbors.size();
            status.phase = territory::Phase::Rendering;
            publish();
          }
          // Navmesh bounds are a requested world square, not an incidental
          // terrain box.
          const int mx = std::stoi(name.substr(0, 2)),
                    my = std::stoi(name.substr(3, 2));
          const auto bounds = region ? region->geometry.game_bounds : context.maps.front().bounding_box();
          if (std::abs(bounds.min().x - (mx - 20) * 32768.f) > .1f ||
              std::abs(bounds.min().y - (my - 18) * 32768.f) > .1f ||
              std::abs(bounds.max().x - (mx - 19) * 32768.f) > .1f ||
              std::abs(bounds.max().y - (my - 17) * 32768.f) > .1f)
            throw std::runtime_error(
                "Navmesh geometry bounds do not match requested square");
          auto last = std::chrono::steady_clock::time_point{};
          const auto progress = [&](std::size_t done, std::size_t total) {
                status.tiles_done = done;
                status.tiles_total = total;
                const auto now = std::chrono::steady_clock::now();
                if (done == 0 || done == total ||
                    now - last > std::chrono::milliseconds(250)) {
                  publish();
                  last = now;
                }
              };
          auto result = region ? navmesh::build(region->geometry, job.navmesh_settings, cancelled, progress)
                               : navmesh::build(context.maps.front(), job.navmesh_settings, cancelled, progress);
          territory::check_cancel(cancelled);
          status.phase = territory::Phase::Saving;
          publish();
          const auto filename = name + ".navmesh";
          navmesh::save(*result.mesh, directory / filename, cancelled);
          files.push_back(filename);
          if (region) {
            verify_navmesh_sources(job.client, region->metadata, cancelled);
            region->metadata.mesh=territory::file_identity(directory/filename);
            navmesh::write_region_metadata_new(region->metadata,directory/filename,cancelled);
            files.push_back(filename+".json");
          }
          navmesh_reports.push_back(
              {{"map", name},
               {"file", filename},
               {"tiles", result.tiles},
               {"polygons", result.polygons},
               {"empty_tiles", result.empty_tiles},
               {"bytes", std::filesystem::file_size(directory / filename)},
               {"seconds", std::chrono::duration<double>(
                               std::chrono::steady_clock::now() - started)
                               .count()},
               {"game_bounds",
                {bounds.min().x, bounds.min().y, bounds.max().x,
                 bounds.max().y}},
               {"neighbor_context", job.navmesh_neighbor_context},
               {"missing_neighbors", region ? region->metadata.missing_neighbors : std::vector<std::string>{}},
               {"world_seams_verified", false}});
          if (!region) ++status.warnings;
        }
        ++status.map_index;
        publish();
      }
      territory::check_cancel(cancelled);
      status.phase = territory::Phase::Completed;
    } catch (const navmesh::Cancelled &) {
      status.phase = territory::Phase::Cancelled;
      status.error = "Cancelled";
      code = 130;
    } catch (const territory::Cancelled &) {
      status.phase = territory::Phase::Cancelled;
      status.error = "Cancelled";
      code = 130;
    } catch (const std::exception &error) {
      status.phase = territory::Phase::Failed;
      status.error = error.what();
      std::cerr << "Geodata build failed: " << error.what() << '\n';
      code = 3;
    }
    auto report = geodata_job_json(job);
    report["result"] = territory::to_json(status);
    report["files"] = files;
    report["generator"] = build_identity;
    if (job.navmesh) {
      report["navmesh"] = navmesh_reports;
      report["navmesh_format"] = {
          {"container", "MSET"},
          {"container_version", 1},
          {"tile_version", 7},
          {"poly_ref_bits", 64},
          {"verts_per_poly", 6},
          {"byte_order", "little"},
          {"game_to_nav_axes", "x,z,y"},
          {"world_units_per_nav_unit", 1},
          {"recast_revision", "c187b7e+libs/patches/recast.patch"},
          {"warning", "Experimental: inter-region seams, off-mesh movement and "
                      "server semantics not verified"}};
    }
    territory::write_json_atomic(directory / "report.json", report, false);
    publish();
    return code;
  } catch (const std::exception &error) {
    std::cerr << "Invalid geodata job: " << error.what() << '\n';
    return 2;
  }
}
