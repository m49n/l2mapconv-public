#include "ExecutablePath.h"

#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif

auto running_executable_path() -> std::filesystem::path {
#if defined(_WIN32)
  std::vector<wchar_t> buffer(260);

  for (;;) {
    const auto length = GetModuleFileNameW(nullptr, buffer.data(),
                                           static_cast<DWORD>(buffer.size()));
    if (length == 0) {
      throw std::runtime_error{"Could not resolve the executable path"};
    }
    if (length < buffer.size()) {
      return std::filesystem::path{
          std::wstring_view{buffer.data(), static_cast<std::size_t>(length)}};
    }
    buffer.resize(buffer.size() * 2);
  }
#elif defined(__APPLE__)
  std::uint32_t size = 0;
  _NSGetExecutablePath(nullptr, &size);
  std::vector<char> buffer(size);
  if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
    throw std::runtime_error{"Could not resolve the executable path"};
  }
  return std::filesystem::weakly_canonical(buffer.data());
#else
  std::vector<char> buffer(256);

  for (;;) {
    const auto length =
        readlink("/proc/self/exe", buffer.data(), buffer.size());
    if (length < 0) {
      throw std::runtime_error{"Could not resolve the executable path"};
    }
    if (static_cast<std::size_t>(length) < buffer.size()) {
      return std::filesystem::path{
          std::string_view{buffer.data(), static_cast<std::size_t>(length)}};
    }
    buffer.resize(buffer.size() * 2);
  }
#endif
}
