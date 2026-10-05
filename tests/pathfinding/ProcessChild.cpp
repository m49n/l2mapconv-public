#define NOMINMAX
#include "TerritoryProcess.h"
#include <filesystem>
#include <iostream>
#include <territory/PathIO.h>
#include <windows.h>

int wmain(int argc, wchar_t **argv) try {
  if (argc < 3)
    return 2;
  const std::filesystem::path output = argv[2];
  if (std::wstring_view(argv[1]) == L"--echo") {
    auto args = territory::Json::array();
    for (int i = 3; i < argc; ++i)
      args.push_back(territory::path_utf8(std::filesystem::path{argv[i]}));
    territory::write_json_atomic(output, args, false);
    std::cout << "child log marker\n";
    return 7;
  }
  if (std::wstring_view(argv[1]) == L"--hold") {
    territory::write_json_atomic(output, {{"pid", GetCurrentProcessId()}},
                                 false);
    Sleep(30000);
    return 0;
  }
  if (std::wstring_view(argv[1]) == L"--tree") {
    wchar_t self[32768]{};
    GetModuleFileNameW(nullptr, self, 32768);
    auto command = quote_windows_argument(self) + L" --hold " +
                   quote_windows_argument(output.wstring());
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(self, command.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
      return 3;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    Sleep(30000);
    return 0;
  }
  return 2;
} catch (const std::exception &e) {
  std::cerr << e.what() << '\n';
  return 3;
}
