#include "OwnedChildProcess.h"
#include "TerritoryProcess.h"
#include <stdexcept>
#include <system_error>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
namespace {
struct Handle {
  HANDLE value{};
  Handle() = default;
  explicit Handle(HANDLE h) : value(h) {}
  ~Handle() { reset(); }
  void reset(HANDLE h = nullptr) {
    if (value && value != INVALID_HANDLE_VALUE)
      CloseHandle(value);
    value = h;
  }
  Handle(const Handle &) = delete;
};
[[noreturn]] void fail(const char *what, DWORD error = GetLastError()) {
  throw std::system_error(static_cast<int>(error), std::system_category(),
                          what);
}
} // namespace
struct OwnedChildProcess::Impl {
  // Destroy job first: descendants cannot outlive this owner.
  Handle process, job;
};
#else
struct OwnedChildProcess::Impl {};
#endif
OwnedChildProcess::OwnedChildProcess() : m_impl(std::make_unique<Impl>()) {}
OwnedChildProcess::~OwnedChildProcess() = default;
void OwnedChildProcess::start(const std::filesystem::path &exe,
                              const std::vector<std::wstring> &arguments,
                              const std::filesystem::path &log) {
#ifdef _WIN32
  if (m_impl->process.value && !exit_code())
    throw std::logic_error("Owned child is already running");
  if (!exe.is_absolute() || !log.is_absolute())
    throw std::invalid_argument("Owned child paths must be absolute");
  auto candidate = std::make_unique<Impl>();
  candidate->job.reset(CreateJobObjectW(nullptr, nullptr));
  if (!candidate->job.value)
    fail("CreateJobObject");
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
  limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  if (!SetInformationJobObject(candidate->job.value,
                               JobObjectExtendedLimitInformation, &limits,
                               sizeof(limits)))
    fail("SetInformationJobObject");
  SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
  Handle output{CreateFileW(log.c_str(), GENERIC_WRITE,
                            FILE_SHARE_READ | FILE_SHARE_DELETE, &security,
                            CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr)};
  if (output.value == INVALID_HANDLE_VALUE)
    fail("Create child log");
  Handle input{CreateFileW(L"NUL", GENERIC_READ,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, &security,
                           OPEN_EXISTING, 0, nullptr)};
  if (input.value == INVALID_HANDLE_VALUE)
    fail("Create child stdin");
  SIZE_T bytes = 0;
  InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
  std::vector<std::max_align_t> storage((bytes + sizeof(std::max_align_t) - 1) /
                                        sizeof(std::max_align_t));
  auto *attributes =
      reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
  if (!InitializeProcThreadAttributeList(attributes, 1, 0, &bytes))
    fail("Initialize process attributes");
  struct Attributes {
    LPPROC_THREAD_ATTRIBUTE_LIST value;
    ~Attributes() { DeleteProcThreadAttributeList(value); }
  } owned{attributes};
  HANDLE inherited[]{output.value, input.value};
  if (!UpdateProcThreadAttribute(attributes, 0,
                                 PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited,
                                 sizeof(inherited), nullptr, nullptr))
    fail("Set inherited handles");
  STARTUPINFOEXW startup{};
  startup.StartupInfo.cb = sizeof(startup);
  startup.lpAttributeList = attributes;
  startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
  startup.StartupInfo.hStdInput = input.value;
  startup.StartupInfo.hStdOutput = output.value;
  startup.StartupInfo.hStdError = output.value;
  std::wstring command = quote_windows_argument(exe.wstring());
  for (const auto &arg : arguments) {
    if (arg.find(L'\0') != std::wstring::npos)
      throw std::invalid_argument("NUL in child argument");
    command += L" " + quote_windows_argument(arg);
  }
  if (command.size() >= 32767)
    throw std::invalid_argument("Child command line exceeds Windows limit");
  PROCESS_INFORMATION child{};
  if (!CreateProcessW(
          exe.c_str(), command.data(), nullptr, nullptr, TRUE,
          CREATE_NO_WINDOW | CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT,
          nullptr, log.parent_path().c_str(), &startup.StartupInfo, &child))
    fail("Create owned child");
  candidate->process.reset(child.hProcess);
  Handle thread{child.hThread};
  if (!AssignProcessToJobObject(candidate->job.value, child.hProcess)) {
    const auto error = GetLastError();
    TerminateProcess(child.hProcess, 3);
    fail("Assign owned child", error);
  }
  if (ResumeThread(child.hThread) == DWORD(-1))
    fail("Resume owned child");
  m_impl = std::move(candidate);
#else
  (void)exe;
  (void)arguments;
  (void)log;
  throw std::runtime_error("Owned children require Windows");
#endif
}
auto OwnedChildProcess::exit_code() -> std::optional<int> {
#ifdef _WIN32
  if (!m_impl->process.value)
    throw std::logic_error("No owned child");
  const auto wait = WaitForSingleObject(m_impl->process.value, 0);
  if (wait == WAIT_TIMEOUT)
    return std::nullopt;
  if (wait != WAIT_OBJECT_0)
    fail("Wait for owned child");
  DWORD code{};
  if (!GetExitCodeProcess(m_impl->process.value, &code))
    fail("Get owned child exit code");
  return static_cast<int>(code);
#else
  return 2;
#endif
}
void OwnedChildProcess::terminate_tree_owned() {
#ifdef _WIN32
  if (m_impl->job.value && !TerminateJobObject(m_impl->job.value, 130))
    fail("Terminate owned child tree");
#endif
}
