#include "TerritoryProcess.h"
#include <stdexcept>
#include <system_error>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
std::wstring quote_windows_argument(std::wstring_view argument) {
  std::wstring result = L"\"";
  std::size_t slashes = 0;
  for (wchar_t c : argument) {
    if (c == L'\\') {
      ++slashes;
      continue;
    }
    result.append(slashes * (c == L'"' ? 2 : 1), L'\\');
    slashes = 0;
    if (c == L'"')
      result += L'\\';
    result += c;
  }
  result.append(slashes * 2, L'\\');
  result += L'"';
  return result;
}
namespace {
#ifdef _WIN32
class WindowsProcess final : public TerritoryProcess {
public:
  ~WindowsProcess() override { close(); }
  void start(const std::filesystem::path &exe,
             const std::filesystem::path &directory) override {
    if (process && !exit_code())
      throw std::logic_error("Territory worker is already running");
    close();
    if (!exe.is_absolute() || !directory.is_absolute())
      throw std::invalid_argument("Worker paths must be absolute");
    job = CreateJobObjectW(nullptr, nullptr);
    if (!job)
      fail("CreateJobObject");
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags =
        JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation,
                                 &limits, sizeof(limits)))
      fail("SetInformationJobObject");
    auto command = quote_windows_argument(exe.wstring()) + L" --render-job " +
                   quote_windows_argument(directory.wstring());
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION child{};
    if (!CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr,
                        exe.parent_path().c_str(), &startup, &child))
      fail("CreateProcess");
    process = child.hProcess;
    if (!AssignProcessToJobObject(job, process)) {
      const auto error = GetLastError();
      TerminateProcess(process, 3);
      CloseHandle(child.hThread);
      fail("AssignProcessToJobObject", error);
    }
    if (ResumeThread(child.hThread) == DWORD(-1)) {
      const auto error = GetLastError();
      CloseHandle(child.hThread);
      fail("ResumeThread", error);
    }
    CloseHandle(child.hThread);
  }
  std::optional<int> exit_code() override {
    if (!process)
      throw std::logic_error("No territory process");
    const auto state = WaitForSingleObject(process, 0);
    if (state == WAIT_TIMEOUT)
      return std::nullopt;
    if (state != WAIT_OBJECT_0)
      throw std::system_error(GetLastError(), std::system_category(),
                              "WaitForSingleObject");
    DWORD code = 0;
    if (!GetExitCodeProcess(process, &code))
      throw std::system_error(GetLastError(), std::system_category(),
                              "GetExitCodeProcess");
    return static_cast<int>(code);
  }
  void terminate_owned() override {
    if (job && !TerminateJobObject(job, 130))
      throw std::system_error(GetLastError(), std::system_category(),
                              "TerminateJobObject");
  }

private:
  HANDLE process{}, job{};
  void close() noexcept {
    if (job) {
      CloseHandle(job);
      job = nullptr;
    }
    if (process) {
      CloseHandle(process);
      process = nullptr;
    }
  }
  [[noreturn]] void fail(const char *operation, DWORD error = GetLastError()) {
    close();
    throw std::system_error(error, std::system_category(), operation);
  }
};
#else
class WindowsProcess final : public TerritoryProcess {
  void start(const std::filesystem::path &,
             const std::filesystem::path &) override {
    throw std::runtime_error("Territory worker requires Windows");
  }
  std::optional<int> exit_code() override { return 2; }
  void terminate_owned() override {}
};
#endif
} // namespace
std::unique_ptr<TerritoryProcess> make_territory_process() {
  return std::make_unique<WindowsProcess>();
}
