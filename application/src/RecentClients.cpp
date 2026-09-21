#include "RecentClients.h"

#include <algorithm>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace {

auto normalize(const std::filesystem::path &path)
    -> std::optional<std::filesystem::path> {
  if (path.empty()) {
    return std::nullopt;
  }

  try {
    auto normalized = std::filesystem::absolute(path).lexically_normal();
    while (normalized != normalized.root_path() &&
           normalized.filename().empty()) {
      normalized = normalized.parent_path();
    }
    normalized.make_preferred();
    return normalized;
  } catch (const std::filesystem::filesystem_error &) {
    return std::nullopt;
  }
}

auto equivalent_path(const std::filesystem::path &left,
                     const std::filesystem::path &right) -> bool {
#ifdef _WIN32
  return CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE) ==
         CSTR_EQUAL;
#else
  return left == right;
#endif
}

auto to_utf8(const std::filesystem::path &path) -> std::string {
  const auto value = path.u8string();
  return {value.begin(), value.end()};
}

auto from_utf8(std::string_view value) -> std::filesystem::path {
  const std::u8string utf8{reinterpret_cast<const char8_t *>(value.data()),
                           value.size()};
  return std::filesystem::path{utf8};
}

void remove_temporary(const std::filesystem::path &path) {
  std::error_code ignored;
  std::filesystem::remove(path, ignored);
}

auto replace_file(const std::filesystem::path &source,
                  const std::filesystem::path &destination) -> bool {
#ifdef _WIN32
  return MoveFileExW(source.c_str(), destination.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
  std::error_code error;
  std::filesystem::rename(source, destination, error);
  return !error;
#endif
}

} // namespace

auto RecentClients::load(const std::filesystem::path &settings_file)
    -> RecentClients {
  RecentClients result;
  std::ifstream input{settings_file, std::ios::binary};
  if (!input) {
    return result;
  }

  std::string line;
  if (!std::getline(input, line)) {
    return result;
  }
  if (!line.empty() && line.back() == '\r') {
    line.pop_back();
  }
  if (line != "version=1") {
    return {};
  }

  constexpr std::string_view client_prefix{"client="};
  while (std::getline(input, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (!line.starts_with(client_prefix) ||
        line.size() == client_prefix.size()) {
      return {};
    }

    try {
      const auto path = normalize(
          from_utf8(std::string_view{line}.substr(client_prefix.size())));
      if (!path) {
        return {};
      }
      if (result.m_entries.size() < maximum_entries &&
          std::ranges::none_of(result.m_entries, [&](const auto &entry) {
            return equivalent_path(entry, *path);
          })) {
        result.m_entries.push_back(*path);
      }
    } catch (const std::exception &) {
      return {};
    }
  }

  return result;
}

auto RecentClients::save(const std::filesystem::path &settings_file) const
    -> bool {
  auto temporary_file = settings_file;
  temporary_file += ".tmp";

  try {
    if (!settings_file.parent_path().empty()) {
      std::filesystem::create_directories(settings_file.parent_path());
    }

    std::ofstream output{temporary_file,
                         std::ios::binary | std::ios::out | std::ios::trunc};
    if (!output) {
      remove_temporary(temporary_file);
      return false;
    }

    output << "version=1\n";
    for (const auto &entry : m_entries) {
      output << "client=" << to_utf8(entry) << '\n';
    }
    output.flush();
    if (!output) {
      output.close();
      remove_temporary(temporary_file);
      return false;
    }
    output.close();
    if (!output || !replace_file(temporary_file, settings_file)) {
      remove_temporary(temporary_file);
      return false;
    }
    return true;
  } catch (const std::filesystem::filesystem_error &) {
    remove_temporary(temporary_file);
    return false;
  }
}

void RecentClients::promote(const std::filesystem::path &client_root) {
  const auto normalized = normalize(client_root);
  if (!normalized) {
    return;
  }

  std::erase_if(m_entries, [&](const auto &entry) {
    return equivalent_path(entry, *normalized);
  });
  m_entries.insert(m_entries.begin(), *normalized);
  if (m_entries.size() > maximum_entries) {
    m_entries.resize(maximum_entries);
  }
}

auto RecentClients::entries() const
    -> const std::vector<std::filesystem::path> & {
  return m_entries;
}

auto recent_clients_settings_path(
    const std::optional<std::filesystem::path> &local_app_data)
    -> std::optional<std::filesystem::path> {
  if (!local_app_data) {
    return std::nullopt;
  }
  return *local_app_data / "l2mapconv" / "settings.ini";
}
