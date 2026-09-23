#pragma once
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
class TerritoryProcess {
public:
  virtual ~TerritoryProcess() = default;
  virtual void start(const std::filesystem::path &exe,
                     const std::filesystem::path &job) = 0;
  virtual std::optional<int> exit_code() = 0;
  virtual void terminate_owned() = 0;
};
std::unique_ptr<TerritoryProcess> make_territory_process();
std::wstring quote_windows_argument(std::wstring_view);
