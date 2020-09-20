#pragma once

#include <string>
#include <vector>

class Application {
public:
  explicit Application(const std::vector<std::string> &arguments);

  auto run() -> int;

private:
  const std::vector<std::string> m_arguments;
};
