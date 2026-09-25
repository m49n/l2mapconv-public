#include "Fixtures.h"
#include "TestSupport.h"
#include <algorithm>
#include <atomic>
#include <fstream>
#include <limits>
#include <territory/Job.h>
#include <territory/PathIO.h>
#include <thread>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <winioctl.h>
namespace {
void junction(const std::filesystem::path &link,
              const std::filesystem::path &target) {
  std::filesystem::create_directory(link);
  auto handle = CreateFileW(
      link.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
      FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
  if (handle == INVALID_HANDLE_VALUE)
    throw std::runtime_error("Cannot open junction fixture");
  const std::wstring substitute = L"\\??\\" + target.native();
  const std::wstring print = target.native();
  struct MountPoint {
    DWORD tag;
    WORD length, reserved, substitute_offset, substitute_length, print_offset,
        print_length;
    wchar_t buffer[2048];
  } data{};
  if (substitute.size() + print.size() + 2 > 2048) {
    CloseHandle(handle);
    throw std::runtime_error("junction path too long");
  }
  data.tag = IO_REPARSE_TAG_MOUNT_POINT;
  data.substitute_length =
      static_cast<WORD>(substitute.size() * sizeof(wchar_t));
  data.print_offset = data.substitute_length + sizeof(wchar_t);
  data.print_length = static_cast<WORD>(print.size() * sizeof(wchar_t));
  data.length = 8 + data.print_offset + data.print_length + sizeof(wchar_t);
  std::copy(substitute.begin(), substitute.end(), data.buffer);
  std::copy(print.begin(), print.end(), data.buffer + substitute.size() + 1);
  DWORD bytes = 0;
  bool ok = DeviceIoControl(handle, FSCTL_SET_REPARSE_POINT, &data,
                            data.length + 8, nullptr, 0, &bytes, nullptr);
  auto error = GetLastError();
  CloseHandle(handle);
  if (!ok)
    throw std::system_error(static_cast<int>(error), std::system_category(),
                            "junction fixture");
}
} // namespace
#endif

int job_tests() {
  using namespace territory;
  int failures = 0;
  TestDirectory fixture;
  auto temp = fixture.path();
  auto client = temp / "client";
  std::filesystem::create_directories(client / "Maps");
  for (int n : {1024, 2048, 4096, 8192, 16384})
    failures += expect(!throws([&] { validate_settings({n, true}); }),
                       "accept render resolution preset");
  failures += expect(throws([] { validate_settings({1234, true}); }),
                     "reject unsupported resolution");
  failures += expect(throws([] { validate_settings({4095, true}); }),
                     "reject resize-like resolution");
  Job j{"job-a", temp / "run", client, {"22_22"}, Mode::Render, {8192, true}};
  validate_job(j);
  {
    auto old_request = to_json(j);
    for (auto key : {"textures", "shadows", "sun_azimuth_deg",
                     "sun_elevation_deg"})
      old_request.erase(key);
    auto old_job = job_from_json(old_request, j.directory);
    auto defaults = to_json(old_job);
    failures += expect(defaults.value("textures", false) &&
                           !defaults.value("shadows", true) &&
                           defaults.value("sun_azimuth_deg", 0.0) == 315.0 &&
                           defaults.value("sun_elevation_deg", 0.0) == 40.0,
                       "old request retains visual defaults");
    Settings custom{4096, false, false, true, 120.0, 55.0};
    auto modified = j;
    modified.settings = custom;
    auto saved_settings = to_json(modified);
    auto restored = to_json(job_from_json(saved_settings, j.directory));
    failures += expect(restored["water"] == false &&
                           restored["textures"] == false &&
                           restored["shadows"] == true &&
                           restored["sun_azimuth_deg"] == 120.0 &&
                           restored["sun_elevation_deg"] == 55.0,
                       "render appearance settings roundtrip exactly");
    for (auto key : {"textures", "shadows"}) {
      auto invalid = saved_settings;
      invalid[key] = 1;
      failures += expect(throws([&] { job_from_json(invalid, j.directory); }),
                         "reject non-boolean render switch");
    }
    for (auto [key, value] :
         {std::pair{"sun_azimuth_deg", Json(-1)},
          {"sun_azimuth_deg", Json(361)},
          {"sun_azimuth_deg", Json("east")},
          {"sun_elevation_deg", Json(14)},
          {"sun_elevation_deg", Json(81)},
          {"sun_elevation_deg", Json("high")}}) {
      auto invalid = saved_settings;
      invalid[key] = value;
      failures += expect(throws([&] { job_from_json(invalid, j.directory); }),
                         "reject invalid sun angle in request");
    }
    custom.sun_azimuth_deg = std::numeric_limits<double>::infinity();
    failures += expect(throws([&] { validate_settings(custom); }),
                       "reject infinite sun azimuth");
    custom.sun_azimuth_deg = 120.0;
    custom.sun_elevation_deg = std::numeric_limits<double>::quiet_NaN();
    failures += expect(throws([&] { validate_settings(custom); }),
                       "reject NaN sun elevation");
  }
  for (int n : {1024, 2048}) {
    auto preview = j;
    preview.settings.resolution = n;
    bool roundtrip = false;
    try {
      roundtrip = job_from_json(to_json(preview), preview.directory)
                      .settings.resolution == n;
    } catch (const std::exception &) {
    }
    failures +=
        expect(roundtrip, "low-resolution request survives JSON roundtrip");
  }
  for (const auto &field : {"id", "maps", "client"}) {
    Job invalid = j;
    if (field == std::string_view("id"))
      invalid.id.clear();
    if (field == std::string_view("maps"))
      invalid.maps.clear();
    if (field == std::string_view("client"))
      invalid.client.clear();
    failures += expect(throws([&] { validate_job(invalid); }),
                       "reject empty required job field");
  }
  for (auto map : {"../22_22", "22_22.unr", "22/22", "2_22", "ab_cd"}) {
    auto invalid = j;
    invalid.maps = {map};
    failures += expect(throws([&] { validate_job(invalid); }),
                       "reject malformed/traversal map");
  }
  auto saved = to_json(j);
  j.maps.push_back("24_18");
  failures += expect(job_from_json(saved, temp / "run").maps ==
                         std::vector<std::string>{"22_22"},
                     "request snapshots selected maps");
  j.maps = {"25_19", "22_22", "25_19"};
  failures += expect(job_from_json(to_json(j), j.directory).maps ==
                         std::vector<std::string>({"22_22", "25_19"}),
                     "normalize sorted unique maps");
  saved["schema_version"] = 99;
  failures += expect(throws([&] { job_from_json(saved, j.directory); }),
                     "reject foreign schema");
  saved = to_json(j);
  saved.erase("client_root");
  failures += expect(throws([&] { job_from_json(saved, j.directory); }),
                     "reject missing client field");
  saved = to_json(j);
  saved["resolution"] = 8192.5;
  failures += expect(throws([&] { job_from_json(saved, j.directory); }),
                     "reject fractional resolution");
  saved = to_json(j);
  saved["water"] = 1;
  failures += expect(throws([&] { job_from_json(saved, j.directory); }),
                     "reject non-boolean water");
  failures += expect(throws([] { path_from_utf8(std::string("\xc3\x28", 2)); }),
                     "reject invalid UTF-8 path");
  Status s;
  s.job_id = "job-a";
  failures += expect(throws([&] { status_from_json(to_json(s), "job-b"); }),
                     "reject foreign status identity");
  s.files = {"../not-owned.png"};
  failures += expect(throws([&] { status_from_json(to_json(s), "job-a"); }),
                     "reject status path escape");
  s.files.clear();
  auto bad_status = to_json(s);
  bad_status["tiles_done"] = -1;
  failures += expect(throws([&] { status_from_json(bad_status, "job-a"); }),
                     "reject negative progress");
  failures += expect(!is_within(temp / "client-old", client),
                     "component boundary not string prefix");
  failures += expect(is_within(client / "Maps" / "new", client),
                     "resolve absent descendants");
  failures += expect(
      throws([&] { create_job_directory(client / "Maps" / "output", client); }),
      "no output under client");
#ifdef _WIN32
  failures += expect(is_within(temp / "CLIENT" / "Maps", client),
                     "Windows case-insensitive containment");
  junction(temp / "link-to-client", client);
  failures +=
      expect(throws([&] {
               create_job_directory(temp / "link-to-client" / "output", client);
             }),
             "junction cannot bypass client output guard");
  failures += expect(!std::filesystem::exists(client / "output"),
                     "rejected junction output creates nothing in client");
  std::filesystem::remove(temp / "link-to-client");
#endif
  auto unicode = path_from_utf8("results \xd1\x82\xd0\xb5\xd1\x81\xd1\x82");
  failures +=
      expect(path_utf8(unicode) == "results \xd1\x82\xd0\xb5\xd1\x81\xd1\x82",
             "UTF-8 filename roundtrip");
  auto run = create_job_directory(temp / unicode / "absent" / "output", client);
  auto run2 =
      create_job_directory(temp / unicode / "absent" / "output", client);
  failures += expect(run != run2 && std::filesystem::is_directory(run),
                     "exclusive unique run directories");
  std::ofstream(temp / "file") << "not a directory";
  failures +=
      expect(throws([&] { create_job_directory(temp / "file", client); }),
             "output file collision rejects");
  write_json_atomic(run / "request.json", to_json(j), false);
  failures +=
      expect(throws([&] {
               write_json_atomic(run / "request.json", to_json(j), false);
             }),
             "request never overwrites");
  failures +=
      expect(throws([&] {
               write_json_atomic(run / "request.json", to_json(j), true);
             }),
             "replacement limited to status");
  for (int i = 0; i < 10; ++i) {
    s.tiles_total = 10;
    s.tiles_done = static_cast<std::size_t>(i);
    write_json_atomic(run / "status.json", to_json(s), true);
    failures += expect(
        status_from_json(read_json(run / "status.json"), "job-a").tiles_done ==
            static_cast<std::size_t>(i),
        "complete atomic status replacement");
  }
  std::atomic_bool reading{true};
  std::atomic_int read_errors{0};
  std::thread reader([&] {
    while (reading) {
      try {
        (void)status_from_json(read_json(run / "status.json"), "job-a");
      } catch (const std::exception &e) {
        if (++read_errors == 1)
          std::cerr << "Concurrent read: " << e.what() << '\n';
      }
    }
  });
  int write_errors = 0;
  for (int i = 0; i < 200; ++i) {
    try {
      write_json_atomic(run / "status.json", to_json(s), true);
    } catch (const std::exception &e) {
      if (++write_errors == 1)
        std::cerr << "Concurrent write: " << e.what() << '\n';
    }
  }
  reading = false;
  reader.join();
  if (read_errors || write_errors)
    std::cerr << "Concurrent status failures: read=" << read_errors
              << ", write=" << write_errors << '\n';
  failures +=
      expect(write_errors == 0 && read_errors == 0,
             "concurrent status reading never blocks atomic publication");
  auto foreign = s;
  foreign.job_id = "another-job";
  failures +=
      expect(throws([&] {
               write_json_atomic(run / "status.json", to_json(foreign), true);
             }),
             "status cannot replace foreign run");
  std::ofstream(run / "status.json", std::ios::trunc)
      << "{\"state\":\"completed\"";
  failures +=
      expect(throws([&] {
               status_from_json(read_json(run / "status.json"), "job-a");
             }),
             "truncated status is not success");
  failures += expect(!cancellation_requested(run), "not initially cancelled");
  request_cancel(run);
  request_cancel(run);
  failures += expect(cancellation_requested(run),
                     "cancel signal is durable and idempotent");
  failures += expect(throws([] { check_cancel([] { return true; }); }),
                     "cooperative cancellation throws");
  check_cancel({});
  check_cancel([] { return false; });
  return failures;
}
