#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace utils {

class Bitset {
public:
  auto size() const -> std::size_t;
  auto operator[](std::size_t index) const -> bool;

  void insert(const std::vector<std::uint8_t> &data);

private:
  std::vector<std::uint8_t> m_data;
};

} // namespace utils
