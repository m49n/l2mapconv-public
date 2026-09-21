#pragma once

#include <iostream>
#include <string_view>

inline auto expect(bool condition, std::string_view message) -> int {
  if (condition) {
    return 0;
  }

  std::cerr << "FAIL: " << message << '\n';
  return 1;
}
