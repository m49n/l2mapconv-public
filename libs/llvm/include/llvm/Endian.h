//===- Endian.h - Utilities for IO with endian specific data ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares generic functions to read and write endian specific data.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SUPPORT_ENDIAN_H
#define LLVM_SUPPORT_ENDIAN_H

#include "SwapByteOrder.h"

namespace llvm {

enum endianness { big, little };
enum alignment { unaligned, aligned };

namespace detail {

template <typename value_type, alignment align> struct alignment_access_helper;

template <typename value_type>
struct alignment_access_helper<value_type, aligned> {
  value_type value;
};

// Provides unaligned loads and stores.
#pragma pack(push)
#pragma pack(1)
template <typename value_type>
struct alignment_access_helper<value_type, unaligned> {
  value_type value;
};
#pragma pack(pop)

} // end namespace detail

namespace endian {

constexpr endianness system_endianness() {
  return sys::IsBigEndianHost ? big : little;
}

template <typename value_type>
inline value_type byte_swap(value_type value, endianness endian) {
  if (endian != system_endianness())
    sys::swapByteOrder(value);
  return value;
}

template <typename value_type, endianness endian>
inline value_type byte_swap(value_type value) {
  return byte_swap(value, endian);
}

template <typename value_type, alignment align>
inline value_type read(const void *memory, endianness endian) {
  value_type value =
      reinterpret_cast<
          const detail::alignment_access_helper<value_type, align> *>(memory)
          ->value;
  return byte_swap<value_type>(value, endian);
}

template <typename value_type, endianness endian, alignment align>
inline value_type read(const void *memory) {
  return read<value_type, align>(memory, endian);
}

} // namespace endian

namespace detail {

template <typename value_type, endianness endian, alignment align>
class packed_endian_specific_integral;

template <typename value_type, endianness endian>
class packed_endian_specific_integral<value_type, endian, unaligned> {
public:
  uint8_t value[sizeof(value_type)];

  operator value_type() const {
    return endian::read<value_type, endian, unaligned>(value);
  }
};

template <typename value_type, endianness endian>
class packed_endian_specific_integral<value_type, endian, aligned> {
public:
  value_type value;

  operator value_type() const {
    return endian::read<value_type, endian, aligned>(&value);
  }
};

} // end namespace detail

using ulittle8_t =
    detail::packed_endian_specific_integral<uint8_t, little, unaligned>;
using ulittle16_t =
    detail::packed_endian_specific_integral<uint16_t, little, unaligned>;
using ulittle32_t =
    detail::packed_endian_specific_integral<uint32_t, little, unaligned>;
using ulittle64_t =
    detail::packed_endian_specific_integral<uint64_t, little, unaligned>;

using little8_t =
    detail::packed_endian_specific_integral<int8_t, little, unaligned>;
using little16_t =
    detail::packed_endian_specific_integral<int16_t, little, unaligned>;
using little32_t =
    detail::packed_endian_specific_integral<int32_t, little, unaligned>;
using little64_t =
    detail::packed_endian_specific_integral<int64_t, little, unaligned>;

using aligned_ulittle8_t =
    detail::packed_endian_specific_integral<uint8_t, little, aligned>;
using aligned_ulittle16_t =
    detail::packed_endian_specific_integral<uint16_t, little, aligned>;
using aligned_ulittle32_t =
    detail::packed_endian_specific_integral<uint32_t, little, aligned>;
using aligned_ulittle64_t =
    detail::packed_endian_specific_integral<uint64_t, little, aligned>;

using aligned_little8_t =
    detail::packed_endian_specific_integral<int8_t, little, aligned>;
using aligned_little16_t =
    detail::packed_endian_specific_integral<int16_t, little, aligned>;
using aligned_little32_t =
    detail::packed_endian_specific_integral<int32_t, little, aligned>;
using aligned_little64_t =
    detail::packed_endian_specific_integral<int64_t, little, aligned>;

using ubig8_t =
    detail::packed_endian_specific_integral<uint8_t, big, unaligned>;
using ubig16_t =
    detail::packed_endian_specific_integral<uint16_t, big, unaligned>;
using ubig32_t =
    detail::packed_endian_specific_integral<uint32_t, big, unaligned>;
using ubig64_t =
    detail::packed_endian_specific_integral<uint64_t, big, unaligned>;

using big8_t = detail::packed_endian_specific_integral<int8_t, big, unaligned>;
using big16_t =
    detail::packed_endian_specific_integral<int16_t, big, unaligned>;
using big32_t =
    detail::packed_endian_specific_integral<int32_t, big, unaligned>;
using big64_t =
    detail::packed_endian_specific_integral<int64_t, big, unaligned>;

using aligned_ubig8_t =
    detail::packed_endian_specific_integral<uint8_t, big, aligned>;
using aligned_ubig16_t =
    detail::packed_endian_specific_integral<uint16_t, big, aligned>;
using aligned_ubig32_t =
    detail::packed_endian_specific_integral<uint32_t, big, aligned>;
using aligned_ubig64_t =
    detail::packed_endian_specific_integral<uint64_t, big, aligned>;

using aligned_big8_t =
    detail::packed_endian_specific_integral<int8_t, big, aligned>;
using aligned_big16_t =
    detail::packed_endian_specific_integral<int16_t, big, aligned>;
using aligned_big32_t =
    detail::packed_endian_specific_integral<int32_t, big, aligned>;
using aligned_big64_t =
    detail::packed_endian_specific_integral<int64_t, big, aligned>;

} // namespace llvm

#endif
