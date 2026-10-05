#pragma once
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Owns only the launched process tree. Closing the owner cancels descendants.
class OwnedChildProcess {
public:
  OwnedChildProcess();
  ~OwnedChildProcess();
  OwnedChildProcess(const OwnedChildProcess &) = delete;
  OwnedChildProcess &operator=(const OwnedChildProcess &) = delete;
  void start(const std::filesystem::path &executable,
             const std::vector<std::wstring> &arguments,
             const std::filesystem::path &log);
  auto exit_code() -> std::optional<int>;
  void terminate_tree_owned();

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};
