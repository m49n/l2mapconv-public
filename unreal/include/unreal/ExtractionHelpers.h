#pragma once

#include <utils/Assert.h>
#include <utils/Log.h>

#include <llvm/Endian.h>

#include <cstdint>
#include <istream>

namespace unreal {

// std::istream packed_endian_specific_integral extraction.
template <typename value_type, llvm::endianness endian, llvm::alignment align>
auto operator>>(
    std::istream &input_stream,
    llvm::detail::packed_endian_specific_integral<value_type, endian, align>
        &integral) -> std::istream & {

  input_stream.read(reinterpret_cast<char *>(integral.value),
                    sizeof(integral.value));
  return input_stream;
}

// Single value extraction.
template <typename ExtractAsT, typename StoreToT> struct ExtractHelper {
  StoreToT &store_to;
  explicit ExtractHelper(StoreToT &store_to) : store_to{store_to} {}
};

template <typename ExtractAsT, typename StoreToT, typename IStreamT>
auto operator>>(IStreamT &input_stream,
                ExtractHelper<ExtractAsT, StoreToT> extract_helper)
    -> IStreamT & {

  ExtractAsT value{};
  input_stream >> value;
  extract_helper.store_to = value;
  return input_stream;
}

template <typename ExtractAsT, typename StoreToT>
auto extract(StoreToT &store_to) -> ExtractHelper<ExtractAsT, StoreToT> {
  return ExtractHelper<ExtractAsT, StoreToT>{store_to};
}

template <typename ExtractSizeAsT, typename ExtractElementAsT,
          typename StoreToT>
struct ExtractArrayHelper {
  StoreToT &store_to;
  explicit ExtractArrayHelper(StoreToT &store_to) : store_to{store_to} {}

  template <typename IStreamT> auto read(IStreamT &input_stream) -> IStreamT & {
    ExtractSizeAsT size_value{};
    input_stream >> size_value;
    const std::int32_t size = size_value;

    ASSERT(size >= 0, "Unreal", "Size can't be negative");
    store_to.reserve(size);

    for (auto i = 0; i < size; ++i) {
      ExtractElementAsT element{};
      input_stream >> element;
      store_to.push_back(element);
    }

    return input_stream;
  }
};

template <typename ExtractSizeAsT, typename StoreToT>
struct ExtractArrayHelper<ExtractSizeAsT, llvm::ulittle8_t, StoreToT> {
  StoreToT &store_to;
  explicit ExtractArrayHelper(StoreToT &store_to) : store_to{store_to} {}

  template <typename IStreamT> auto read(IStreamT &input_stream) -> IStreamT & {
    ExtractSizeAsT size_value{};
    input_stream >> size_value;
    const std::int32_t size = size_value;

    ASSERT(size >= 0, "Unreal", "Size can't be negative");
    store_to.resize(size);

    if (size > 0) {
      static_cast<std::istream &>(input_stream)
          .read(reinterpret_cast<char *>(&store_to.front()), size);
    }

    return input_stream;
  }
};

template <typename ExtractSizeAsT, typename PESIT, llvm::endianness PESIE,
          llvm::alignment PESIA, typename StoreToT>
struct ExtractArrayHelper<
    ExtractSizeAsT,
    llvm::detail::packed_endian_specific_integral<PESIT, PESIE, PESIA>,
    StoreToT> {

  StoreToT &store_to;
  explicit ExtractArrayHelper(StoreToT &store_to) : store_to{store_to} {}

  template <typename IStreamT> auto read(IStreamT &input_stream) -> IStreamT & {
    ExtractSizeAsT size_value{};
    input_stream >> size_value;
    const std::int32_t size = size_value;

    ASSERT(size >= 0, "Unreal", "Size can't be negative");
    store_to.resize(size);

    if (size > 0) {
      static_cast<std::istream &>(input_stream)
          .read(reinterpret_cast<char *>(&store_to.front()),
                size * sizeof(PESIT));
    }

    return input_stream;
  }
};

template <typename ExtractSizeAsT, typename ExtractElementAsT,
          typename StoreToT, typename IStreamT>
auto operator>>(IStreamT &input_stream,
                ExtractArrayHelper<ExtractSizeAsT, ExtractElementAsT, StoreToT>
                    extract_array_helper) -> IStreamT & {

  return extract_array_helper.read(input_stream);
}

template <typename ExtractSizeAsT, typename ExtractElementAsT,
          typename StoreToT>
auto extract_array(StoreToT &store_to)
    -> ExtractArrayHelper<ExtractSizeAsT, ExtractElementAsT, StoreToT> {

  return ExtractArrayHelper<ExtractSizeAsT, ExtractElementAsT, StoreToT>(
      store_to);
}

} // namespace unreal
