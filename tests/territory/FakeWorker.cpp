#include <chrono>
#include <territory/PathIO.h>
#include <thread>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>

#include <shellapi.h>
#endif
int main() {
#ifdef _WIN32
  int argc = 0;
  auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  if (!argv)
    return 2;
  struct Free {
    wchar_t **p;
    ~Free() { LocalFree(p); }
  } free{argv};
  if (argc >= 3 && std::wstring_view(argv[1]) == L"--echo") {
    territory::Json values = territory::Json::array();
    for (int n = 3; n < argc; ++n)
      values.push_back(territory::path_utf8(std::filesystem::path(argv[n])));
    territory::write_json_atomic(argv[2], values, false);
    return 0;
  }
  if (argc == 3 && std::wstring_view(argv[1]) == L"--exit")
    return std::stoi(argv[2]);
  if (argc == 2 && std::wstring_view(argv[1]) == L"--hang")
    for (;;)
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
  if (argc != 3)
    return 2;
  const std::filesystem::path directory = argv[2];
  auto mode = std::wstring_view(argv[1]);
  if (mode == L"--render-job") {
    const auto control =
        territory::read_json(directory / "fixture-control.json");
    territory::write_json_atomic(
        directory / "fixture-started.json",
        {{"pid", GetCurrentProcessId()},
         {"directory", territory::path_utf8(directory)}},
        false);
    if (control.value("mode", "") == "exit")
      return control.value("code", 0);
    if (control.value("mode", "") == "crash") {
      TerminateProcess(GetCurrentProcess(), 77);
      return 77;
    }
    if (control.value("mode", "") == "hang")
      for (;;)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
  } else if (mode != L"--wait-cancel")
    return 2;
  for (;;) {
    if (territory::cancellation_requested(directory))
      return 130;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
#else
  return 2;
#endif
}
