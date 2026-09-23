#include <algorithm>
#include <fstream>
#include <random>
#include <stdexcept>
#include <system_error>
#include <territory/Job.h>
#include <territory/PathIO.h>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
namespace territory {
namespace {
std::string token() {
  std::random_device r;
  return std::to_string(r()) + "-" + std::to_string(r()) + "-" +
         std::to_string(r());
}
#ifdef _WIN32
class Handle {
public:
  explicit Handle(HANDLE h) : value(h) {}
  ~Handle() {
    if (value != INVALID_HANDLE_VALUE)
      CloseHandle(value);
  }
  Handle(const Handle &) = delete;
  HANDLE value;
};
[[noreturn]] void windows_error(const char *what) {
  throw std::system_error(static_cast<int>(GetLastError()),
                          std::system_category(), what);
}
#endif
std::filesystem::path resolved(const std::filesystem::path &input) {
  if (input.empty())
    throw std::invalid_argument("empty path");
  auto p = std::filesystem::absolute(input).lexically_normal();
#ifdef _WIN32
  // Resolve junctions using the real existing ancestor, then append absent
  // components.
  std::vector<std::filesystem::path> missing;
  while (!std::filesystem::exists(p)) {
    if (p == p.root_path())
      throw std::invalid_argument("path has no existing root");
    missing.push_back(p.filename());
    p = p.parent_path();
  }
  Handle h(CreateFileW(
      p.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
      nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
  if (h.value == INVALID_HANDLE_VALUE)
    windows_error("resolve path");
  DWORD length =
      GetFinalPathNameByHandleW(h.value, nullptr, 0, FILE_NAME_NORMALIZED);
  if (!length)
    windows_error("resolve final path");
  std::wstring name(length, L'\0');
  DWORD copied = GetFinalPathNameByHandleW(h.value, name.data(), length,
                                           FILE_NAME_NORMALIZED);
  if (!copied || copied >= length)
    windows_error("resolve final path");
  name.resize(copied);
  if (name.starts_with(L"\\\\?\\UNC\\"))
    name = L"\\\\" + name.substr(8);
  else if (name.starts_with(L"\\\\?\\"))
    name.erase(0, 4);
  p = name;
  for (auto it = missing.rbegin(); it != missing.rend(); ++it)
    p /= *it;
  return p.lexically_normal();
#else
  return std::filesystem::weakly_canonical(p);
#endif
}
bool component_equal(const std::filesystem::path &a,
                     const std::filesystem::path &b) {
#ifdef _WIN32
  return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_EQUAL;
#else
  return a == b;
#endif
}
} // namespace
std::string path_utf8(const std::filesystem::path &p) {
  auto text = p.u8string();
  return {reinterpret_cast<const char *>(text.data()), text.size()};
}
std::filesystem::path path_from_utf8(std::string_view s) {
  if (s.find('\0') != std::string_view::npos)
    throw std::invalid_argument("NUL in path");
  // dump performs strict UTF-8 validation, including on POSIX builds.
  (void)Json(std::string(s)).dump();
  return std::filesystem::path(
      std::u8string(reinterpret_cast<const char8_t *>(s.data()), s.size()));
}
bool is_within(const std::filesystem::path &child,
               const std::filesystem::path &parent) {
  auto c = resolved(child), p = resolved(parent);
  auto ci = c.begin();
  for (auto pi = p.begin(); pi != p.end(); ++pi, ++ci) {
    if (pi->empty())
      continue;
    if (ci == c.end() || !component_equal(*ci, *pi))
      return false;
  }
  return true;
}
std::filesystem::path
create_job_directory(const std::filesystem::path &output,
                     const std::filesystem::path &client) {
  if (!std::filesystem::is_directory(client))
    throw std::invalid_argument("client directory does not exist");
  if (is_within(output, client))
    throw std::invalid_argument("output must be outside client");
  auto parent = resolved(output);
  std::filesystem::create_directories(parent);
  parent = resolved(parent);
  if (is_within(parent, client))
    throw std::invalid_argument("output resolves inside client");
  for (int i = 0; i < 100; ++i) {
    auto run = parent / ("render-" + token());
    if (std::filesystem::create_directory(run))
      return run;
  }
  throw std::runtime_error("unable to create unique render directory");
}
void write_json_atomic(const std::filesystem::path &path, const Json &value,
                       bool replace) {
  if (replace) {
    if (path.filename() != "status.json")
      throw std::invalid_argument("only owned status may be replaced");
    const auto id = value.at("job_id").get<std::string>();
    (void)status_from_json(value, id);
    if (std::filesystem::exists(path.parent_path() / "request.json") &&
        read_json(path.parent_path() / "request.json").at("job_id") != id)
      throw std::invalid_argument("status does not belong to request");
    if (std::filesystem::exists(path) && read_json(path).at("job_id") != id)
      throw std::invalid_argument("status does not belong to job");
  }
  const std::string bytes = value.dump(2) + '\n';
  const auto temporary = path.parent_path() / (".json-" + token() + ".tmp");
#ifdef _WIN32
  bool created = false;
  try {
    {
      Handle file(CreateFileW(temporary.c_str(), GENERIC_WRITE | DELETE,
                              FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr,
                              CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
      if (file.value == INVALID_HANDLE_VALUE)
        windows_error("create JSON temporary");
      created = true;
      std::size_t offset = 0;
      while (offset < bytes.size()) {
        DWORD written = 0;
        DWORD amount = static_cast<DWORD>(
            std::min<std::size_t>(bytes.size() - offset, 1024 * 1024));
        if (!WriteFile(file.value, bytes.data() + offset, amount, &written,
                       nullptr) ||
            written != amount)
          windows_error("write JSON");
        offset += written;
      }
      if (!FlushFileBuffers(file.value))
        windows_error("flush JSON");
      // POSIX replacement leaves any readers of the old version intact.
      // MoveFileEx can reject an open destination even with share-delete.
      const auto destination = std::filesystem::absolute(path).native();
      const auto length = offsetof(FILE_RENAME_INFO, FileName) +
                          (destination.size() + 1) * sizeof(wchar_t);
      std::vector<std::max_align_t> storage(
          (length + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t));
      auto *info = reinterpret_cast<FILE_RENAME_INFO *>(storage.data());
      info->Flags = FILE_RENAME_FLAG_POSIX_SEMANTICS |
                    (replace ? FILE_RENAME_FLAG_REPLACE_IF_EXISTS : 0);
      info->RootDirectory = nullptr;
      info->FileNameLength =
          static_cast<DWORD>(destination.size() * sizeof(wchar_t));
      std::copy(destination.begin(), destination.end(), info->FileName);
      info->FileName[destination.size()] = L'\0';
      if (!SetFileInformationByHandle(file.value, FileRenameInfoEx, info,
                                      static_cast<DWORD>(length)))
        windows_error("publish JSON (requires Windows 10+ rename support)");
    }
  } catch (...) {
    if (created) {
      std::error_code ignored;
      std::filesystem::remove(temporary, ignored);
    }
    throw;
  }
#else
  (void)temporary;
  (void)bytes;
  throw std::runtime_error("Territory output requires Windows in this version");
#endif
}
Json read_json(const std::filesystem::path &path) {
  constexpr std::size_t max_bytes = 16 * 1024 * 1024;
#ifdef _WIN32
  // Share deletion: the writer may atomically rename a new status while this
  // handle continues reading the old complete version.
  Handle file(
      CreateFileW(path.c_str(), GENERIC_READ,
                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                  nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
  if (file.value == INVALID_HANDLE_VALUE)
    windows_error("read JSON");
  LARGE_INTEGER size{};
  if (!GetFileSizeEx(file.value, &size))
    windows_error("size JSON");
  if (size.QuadPart < 0 || size.QuadPart > static_cast<LONGLONG>(max_bytes))
    throw std::runtime_error("JSON exceeds 16 MiB limit");
  std::string bytes(static_cast<std::size_t>(size.QuadPart), '\0');
  DWORD read = 0;
  if (!ReadFile(file.value, bytes.data(), static_cast<DWORD>(bytes.size()),
                &read, nullptr))
    windows_error("read JSON");
  if (read != bytes.size())
    throw std::runtime_error("incomplete JSON read");
#else
  const auto size = std::filesystem::file_size(path);
  if (size > max_bytes)
    throw std::runtime_error("JSON exceeds 16 MiB limit");
  std::ifstream file(path, std::ios::binary);
  if (!file)
    throw std::runtime_error("cannot read JSON");
  std::string bytes(static_cast<std::size_t>(size), '\0');
  file.read(bytes.data(), static_cast<std::streamsize>(size));
  if (!file || file.peek() != std::char_traits<char>::eof())
    throw std::runtime_error("JSON changed during read");
#endif
  return Json::parse(bytes);
}
void request_cancel(const std::filesystem::path &directory) {
#ifdef _WIN32
  auto path = directory / "cancel.request";
  Handle file(CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                          CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
  if (file.value == INVALID_HANDLE_VALUE &&
      GetLastError() != ERROR_FILE_EXISTS &&
      GetLastError() != ERROR_ALREADY_EXISTS)
    windows_error("request cancellation");
#else
  (void)directory;
  throw std::runtime_error("Territory output requires Windows in this version");
#endif
}
bool cancellation_requested(const std::filesystem::path &directory) {
  return std::filesystem::exists(directory / "cancel.request");
}
} // namespace territory
