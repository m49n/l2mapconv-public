#pragma once
#include <filesystem>
#include <string>
namespace territory {
struct FileIdentity {
  std::filesystem::path path;
  std::uintmax_t bytes{};
  std::string sha256;
};
FileIdentity file_identity(const std::filesystem::path &);
} // namespace territory
