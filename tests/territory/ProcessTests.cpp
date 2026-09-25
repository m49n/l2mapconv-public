#include "Fixtures.h"
#include "TerritoryRenderController.h"
#include "TestSupport.h"
#include <fstream>
#include <territory/PathIO.h>
#include <territory/PngOutput.h>
#include <thread>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
namespace {
struct FakeProcess : TerritoryProcess {
  std::optional<int> reported_exit;
  int starts{}, terminations{};
  bool delayed_termination{};
  std::filesystem::path exe, job;
  void start(const std::filesystem::path &e,
             const std::filesystem::path &j) override {
    ++starts;
    exe = e;
    job = j;
  }
  std::optional<int> exit_code() override { return reported_exit; }
  void terminate_owned() override {
    ++terminations;
    if (!delayed_termination) reported_exit = 130;
  }
};
#ifdef _WIN32
std::filesystem::path worker_exe() {
  std::wstring buffer(32768, L'\0');
  const auto size = GetModuleFileNameW(nullptr, buffer.data(),
                                       static_cast<DWORD>(buffer.size()));
  if (!size || size == buffer.size())
    throw std::runtime_error("test executable path");
  buffer.resize(size);
  return std::filesystem::path(buffer).parent_path() /
         "territory_fake_worker.exe";
}
template <class Predicate> bool until(Predicate predicate) {
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(5);
  do {
    if (predicate())
      return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  } while (std::chrono::steady_clock::now() < deadline);
  return false;
}
int native_process_tests(const std::filesystem::path &root) {
  using namespace territory;
  int failures = 0;
  const auto exe = worker_exe();
  auto prepare = [&](const std::string &name, const std::string &mode,
                     int code = 0) {
    auto directory = root / path_from_utf8(name);
    std::filesystem::create_directories(directory);
    write_json_atomic(directory / "fixture-control.json",
                      {{"mode", mode}, {"code", code}}, false);
    return directory;
  };
  // A separate owned test sleeper is deliberately outside the tested worker's
  // Job Object.
  auto unrelated = make_territory_process();
  unrelated->start(exe, prepare("unrelated", "hang"));
  auto hang =
      prepare("unicode-\xD0\xBA\xD0\xB0\xD1\x80\xD1\x82\xD0\xB0 space", "hang");
  DWORD handle_count_before = 0;
  GetProcessHandleCount(GetCurrentProcess(), &handle_count_before);
  HANDLE observation = nullptr;
  {
    auto child = make_territory_process();
    child->start(exe, hang);
    failures +=
        expect(until([&] {
                 return std::filesystem::exists(hang / "fixture-started.json");
               }),
               "native worker starts without shell with Unicode/spaces");
    auto start = read_json(hang / "fixture-started.json");
    failures +=
        expect(path_from_utf8(start["directory"].get<std::string>()) == hang,
               "native decoded directory roundtrip");
    observation = OpenProcess(SYNCHRONIZE, FALSE, start["pid"].get<DWORD>());
    failures += expect(observation != nullptr && !child->exit_code(),
                       "worker is running before destruction");
  }
  if (observation) {
    failures += expect(WaitForSingleObject(observation, 5000) == WAIT_OBJECT_0,
                       "destructor kills only owned worker");
    CloseHandle(observation);
  }
  failures +=
      expect(!unrelated->exit_code(), "unrelated sleeper survives destructor");
  for (auto mode : {"exit", "crash", "wait-cancel", "hang"}) {
    auto dir = prepare(std::string("native-") + mode, mode, 7);
    auto child = make_territory_process();
    child->start(exe, dir);
    failures +=
        expect(until([&] {
                 return std::filesystem::exists(dir / "fixture-started.json");
               }),
               "native fixture ready");
    if (std::string_view(mode) == "wait-cancel")
      request_cancel(dir);
    if (std::string_view(mode) == "hang")
      child->terminate_owned();
    failures += expect(until([&] { return child->exit_code().has_value(); }),
                       "native worker exits within deadline");
    auto code = child->exit_code();
    const int expected = std::string_view(mode) == "exit"    ? 7
                         : std::string_view(mode) == "crash" ? 77
                                                             : 130;
    failures += expect(code && *code == expected,
                       "native exit/crash/cancel codes preserved");
    failures += expect(!unrelated->exit_code(),
                       "unrelated sleeper survives cancellation");
  }
  // Native argv decoding checks embedded quotes and trailing backslashes, which
  // cannot be directory names.
  const auto echo = root / "echo.json";
  const std::vector<std::wstring> values{
      L"", L"a b", L"a\"b", L"C:\\dir\\",
      L"\u043a\u0430\u0440\u0442\u0430 \\\""};
  auto command = quote_windows_argument(exe.wstring()) + L" --echo " +
                 quote_windows_argument(echo.wstring());
  for (auto &value : values)
    command += L" " + quote_windows_argument(value);
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION child{};
  if (CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, FALSE,
                     CREATE_NO_WINDOW, nullptr, nullptr, &startup, &child)) {
    CloseHandle(child.hThread);
    const bool ended =
        WaitForSingleObject(child.hProcess, 5000) == WAIT_OBJECT_0;
    if (!ended)
      TerminateProcess(child.hProcess, 3);
    CloseHandle(child.hProcess);
    failures += expect(ended, "argv echo terminates");
    if (ended) {
      auto decoded = read_json(echo);
      for (std::size_t n = 0; n < values.size(); ++n)
        failures +=
            expect(decoded[n] == path_utf8(std::filesystem::path(values[n])),
                   "wide argv exact roundtrip");
    }
  } else
    failures += expect(false, "native argv echo starts");
  DWORD handle_count_after = 0;
  GetProcessHandleCount(GetCurrentProcess(), &handle_count_after);
  failures += expect(handle_count_after <= handle_count_before,
                     "owned process handles released");
  unrelated->terminate_owned();
  failures += expect(until([&] { return unrelated->exit_code().has_value(); }),
                     "test sleeper cleaned up");
  return failures;
}
#endif
} // namespace
int process_tests() {
  using namespace territory;
  int failures = 0;
  TestDirectory temp;
  auto client = temp.path() / "client";
  std::filesystem::create_directory(client);
  {
    auto legacy = to_json(Job{"old-request", temp.path() / "old-run", client,
                              {"22_22"}, Mode::Render, {4096, true}});
    for (auto key : {"textures", "shadows", "sun_azimuth_deg",
                     "sun_elevation_deg"})
      legacy.erase(key);
    const auto recovered = job_from_json(legacy, temp.path() / "old-run");
    failures += expect(recovered.settings.textures &&
                           !recovered.settings.shadows,
                       "worker accepts request JSON from before appearance options");
  }
  auto clock = std::chrono::steady_clock::time_point{};
  auto make = [&] { return std::make_unique<FakeProcess>(); };
  {
    auto worker=make();auto* observed=worker.get();observed->delayed_termination=true;
    TerritoryRenderController controller(temp.path()/"app.exe",std::move(worker));
    failures+=expect(controller.start(client,{"22_22"},{},temp.path()/"delayed-exit"),"start delayed termination fixture");
    std::ofstream invalid(controller.job_directory()/"status.json",std::ios::trunc);invalid<<"{";invalid.close();
    controller.poll();controller.poll();observed->reported_exit=130;controller.poll();
    failures+=expect(!controller.active() && controller.status()->phase==Phase::Failed,"failed state survives asynchronous OS termination");
  }
  auto fake = make();
  auto *observed = fake.get();
  TerritoryRenderController c(temp.path() / "app.exe", std::move(fake),
                              [&] { return clock; });
  failures +=
      expect(c.start(client, {"22_22"}, {8192, true}, temp.path() / "output"),
             "start accepted");
  failures +=
      expect(!c.start(client, {"24_18"}, {4096, false}, temp.path() / "output"),
             "reject concurrent job");
  // Do not dereference the stub-discarded fake until the start implementation
  // exists.
  if (c.active()) {
    observed->reported_exit = 0;
    c.poll();
    failures += expect(!c.active() && !c.error().empty(),
                       "zero exit without terminal status fails");
    c.cancel();
    failures += expect(!cancellation_requested(observed->job),
                       "cancel after terminal completion is a no-op");
  }
  auto scenario = [&](const std::string &kind) {
    auto f = make();
    auto *p = f.get();
    TerritoryRenderController ctl(temp.path() / "app.exe", std::move(f),
                                  [&] { return clock; });
    auto settings = Settings{4096, false};
    std::vector<std::string> maps{"22_22"};
    if (!ctl.start(client, maps, settings, temp.path() / kind)) {
      failures += expect(false, "controller scenario started");
      return;
    }
    maps[0] = "24_18";
    settings.resolution = 16384;
    auto request = job_from_json(
        read_json(ctl.job_directory() / "request.json"), ctl.job_directory());
    failures += expect(request.maps == std::vector<std::string>{"22_22"} &&
                           request.settings.resolution == 4096 &&
                           !request.settings.water,
                       "immutable job snapshot survives UI edits");
    if (kind == "timeout") {
      ctl.cancel();
      ctl.cancel();
      clock += std::chrono::seconds(6);
      ctl.poll();
      failures += expect(!ctl.active() && p->terminations == 1 &&
                             ctl.status()->phase == Phase::Cancelled,
                         "cancel timeout terminates only owned worker");
    } else if (kind == "foreign") {
      auto status = *ctl.status();
      status.job_id = "foreign";
      std::ofstream file(ctl.job_directory() / "status.json", std::ios::trunc);
      file << to_json(status).dump();
      file.close();
      ctl.poll();
      failures +=
          expect(!ctl.active() && !ctl.error().empty() && p->terminations == 1,
                 "foreign status fails and stops owned process");
    } else if (kind == "truncated") {
      std::ofstream file(ctl.job_directory() / "status.json", std::ios::trunc);
      file << "{";
      file.close();
      ctl.poll();
      failures += expect(!ctl.active() && !ctl.error().empty(),
                         "truncated status is not completion");
    } else if (kind == "early-complete") {
      auto status = *ctl.status();
      status.phase = Phase::Completed;
      status.map_index = 1;
      status.map_count = 1;
      status.files = {"22_22_4096.png", "report.json"};
      write_json_atomic(ctl.job_directory() / "status.json", to_json(status),
                        true);
      ctl.poll();
      failures +=
          expect(ctl.active() && ctl.status()->phase != Phase::Completed,
                 "terminal status before child exit is not reported complete");
      p->reported_exit = 0;
      ctl.poll();
      failures += expect(!ctl.active() && ctl.status()->phase == Phase::Failed,
                         "missing completed output rejects false success");
    }
  };
  for (auto kind : {"timeout", "foreign", "truncated", "early-complete"})
    scenario(kind);
  failures += expect(quote_windows_argument(L"") == L"\"\"",
                     "quote empty Windows argument");
  failures += expect(quote_windows_argument(L"a b") == L"\"a b\"",
                     "quote spaced Windows argument");
  failures += expect(quote_windows_argument(L"a\"b") == L"\"a\\\"b\"",
                     "escape embedded quote");
  failures += expect(quote_windows_argument(L"C:\\dir\\") == L"\"C:\\dir\\\\\"",
                     "double trailing backslash before closing quote");
#ifdef _WIN32
  failures += native_process_tests(temp.path());
#endif
  return failures;
}
