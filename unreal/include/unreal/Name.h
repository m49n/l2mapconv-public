#pragma once

#include <string_view>

namespace unreal {

struct Name : public std::string_view {
  static constexpr auto NONE = "None";

  Name() : std::string_view{} {}
  explicit Name(const char *data, std::string_view::size_type length)
      : std::string_view{data, length} {}
};

} // namespace unreal
