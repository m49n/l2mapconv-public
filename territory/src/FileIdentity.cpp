#include <array>
#include <stdexcept>
#include <system_error>
#include <territory/FileIdentity.h>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>

#include <bcrypt.h>
#endif
namespace territory {
FileIdentity file_identity(const std::filesystem::path &path) {
#ifdef _WIN32
  struct File {
    HANDLE value{INVALID_HANDLE_VALUE};
    ~File() {
      if (value != INVALID_HANDLE_VALUE)
        CloseHandle(value);
    }
  };
  File file{CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                        OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr)};
  if (file.value == INVALID_HANDLE_VALUE)
    throw std::system_error(GetLastError(), std::system_category(),
                            "open hash input");
  BY_HANDLE_FILE_INFORMATION before{}, after{};
  if (!GetFileInformationByHandle(file.value, &before) ||
      (before.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
    throw std::runtime_error("Invalid hash input file");
  auto checked = [](NTSTATUS status, const char *what) {
    if (status < 0)
      throw std::runtime_error(
          std::string(what) +
          " NTSTATUS=" + std::to_string(static_cast<unsigned long>(status)));
  };
  struct Algorithm {
    BCRYPT_ALG_HANDLE value{};
    ~Algorithm() {
      if (value)
        BCryptCloseAlgorithmProvider(value, 0);
    }
  } algorithm;
  struct Hash {
    BCRYPT_HASH_HANDLE value{};
    ~Hash() {
      if (value)
        BCryptDestroyHash(value);
    }
  } hash;
  checked(BCryptOpenAlgorithmProvider(&algorithm.value, BCRYPT_SHA256_ALGORITHM,
                                      nullptr, 0),
          "SHA-256 provider");
  checked(
      BCryptCreateHash(algorithm.value, &hash.value, nullptr, 0, nullptr, 0, 0),
      "SHA-256 create");
  std::array<unsigned char, 65536> buffer{};
  std::uintmax_t total = 0;
  for (;;) {
    DWORD count = 0;
    if (!ReadFile(file.value, buffer.data(), buffer.size(), &count, nullptr))
      throw std::system_error(GetLastError(), std::system_category(),
                              "read hash input");
    if (!count)
      break;
    checked(BCryptHashData(hash.value, buffer.data(), count, 0),
            "SHA-256 update");
    total += count;
  }
  if (!GetFileInformationByHandle(file.value, &after))
    throw std::runtime_error("Cannot recheck hash input");
  const auto size =
      (std::uintmax_t(before.nFileSizeHigh) << 32) | before.nFileSizeLow;
  if (total != size || before.nFileSizeHigh != after.nFileSizeHigh ||
      before.nFileSizeLow != after.nFileSizeLow ||
      CompareFileTime(&before.ftLastWriteTime, &after.ftLastWriteTime))
    throw std::runtime_error("Hash input changed during read");
  std::array<unsigned char, 32> digest{};
  checked(BCryptFinishHash(hash.value, digest.data(), digest.size(), 0),
          "SHA-256 finish");
  const char hex[] = "0123456789abcdef";
  std::string encoded;
  encoded.reserve(64);
  for (auto value : digest) {
    encoded.push_back(hex[value >> 4]);
    encoded.push_back(hex[value & 15]);
  }
  return {std::filesystem::absolute(path).lexically_normal(), total, encoded};
#else
  (void)path;
  throw std::runtime_error(
      "File identities require Windows BCrypt in this version");
#endif
}
} // namespace territory
