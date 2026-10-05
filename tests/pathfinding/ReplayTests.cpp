#include "OwnedChildProcess.h"
#include "PathfindingJob.h"
#include "Support.h"
#include <territory/PathIO.h>
#include <thread>
using namespace pf_test;
int replay_tests(const std::filesystem::path &exe) {
  TempDirectory inputs, outputs;
  pathfinding::RouteCase c{
      "replay",
      "25_19",
      {{166136.25, 53528.5, -4112}, -4112},
      {{166136, 53544, -4112}, -4112},
      pathfinding::identify_file(inputs.file("25_19.l2j", "abc")),
      pathfinding::identify_file(inputs.file("25_19.navmesh", "nav"))};
  const auto saved = outputs.path / "case.json",
             profile = outputs.path / "profile.json";
  pathfinding::write_case_new(c, saved);
  auto profile_data = pathfinding::profile_json({});
  profile_data["legacy_config"] = "geo.json";
  territory::write_json_atomic(profile, profile_data, false);
  int n = 0;
  n += check(
      "public CLI replays a saved case through the same immutable job worker",
      [&] {
        OwnedChildProcess child;
        const auto directory = outputs.path / "replay-job";
        child.start(exe,
                    {L"--pathfinding-case", saved.wstring(),
                     L"--backend-profile", profile.wstring(), L"--output",
                     directory.wstring()},
                    outputs.path / "replay.log");
        for (int i = 0; i < 1000 && !child.exit_code(); ++i)
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (child.exit_code() != 3 ||
            !std::filesystem::exists(directory / "report.json"))
          return false;
        const auto job = pathfinding_job_from_json(
            territory::read_json(directory / "request.json"), directory);
        const auto r = read_pathfinding_report(job);
        return job.profile.legacy_config == outputs.path / "geo.json" &&
               job.route.a.requested == c.a.requested && r.size() == 2 &&
               r[0].status == pathfinding::RouteStatus::BackendUnavailable;
      });
  n += check("changed input remains rejected until explicit identity adoption "
             "and never alters saved case",
             [&] {
               const auto before = pathfinding::identify_file(saved);
               inputs.file("25_19.l2j", "changed");
               const auto original = pathfinding::read_case(saved);
               const auto changed = pathfinding::input_changes(original);
               if (changed.size() != 1 || !rejects([&] {
                     pathfinding::verify_identity(original.l2j);
                   }))
                 return false;
               auto accepted = pathfinding::with_current_identities(original);
               return accepted.id == original.id &&
                      accepted.l2j.sha256 != original.l2j.sha256 &&
                      accepted.a.requested == original.a.requested &&
                      pathfinding::identify_file(saved).sha256 == before.sha256;
             });
  return n;
}
