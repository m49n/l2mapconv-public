#include <utils/Bitset.h>

namespace utils {

auto Bitset::size() const -> std::size_t {
  static constexpr auto bit_count =
      std::numeric_limits<decltype(m_data)::value_type>::digits;
  return m_data.size() * bit_count;
}

auto Bitset::operator[](std::size_t index) const -> bool {
  static constexpr auto bit_count =
      std::numeric_limits<decltype(m_data)::value_type>::digits;
  const auto bits = m_data[index / bit_count];
  const auto bit_index = index % bit_count;
  return (bits & (1 << bit_index)) != 0;
}

void Bitset::insert(const std::vector<std::uint8_t> &data) { m_data = data; }

} // namespace utils
