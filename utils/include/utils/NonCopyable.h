#pragma once

namespace utils {

class NonCopyable {
public:
  NonCopyable(const NonCopyable &) = delete;
  auto operator=(const NonCopyable &) -> NonCopyable & = delete;
  NonCopyable() {}
};

} // namespace utils
