#include "pch.h"

#include <unreal/Archive.h>
#include <unreal/ArrayIndex.h>

namespace unreal {

auto operator>>(Archive &archive, ArrayIndex &array_index) -> Archive & {
  std::uint8_t byte = 0;
  archive >> byte;
  array_index.value = byte;

  if (byte < 128) {
    return archive;
  }

  if ((byte & 0x80) != 0) {
    archive >> byte;
    array_index.value |= static_cast<std::uint32_t>(byte) << 8;
  } else if ((byte & 0xC0) != 0) {
    archive >> byte;
    array_index.value |= static_cast<std::uint32_t>(byte) << 8;
    archive >> byte;
    array_index.value |= static_cast<std::uint32_t>(byte) << 16;
    archive >> byte;
    array_index.value |= static_cast<std::uint32_t>(byte) << 24;
  }

  return archive;
}

} // namespace unreal
