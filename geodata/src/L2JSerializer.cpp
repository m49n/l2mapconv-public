#include "pch.h"

#include "L2JSerializer.h"

#include <utils/ExtractionHelpers.h>

namespace geodata {

auto L2JSerializer::deserialize(std::istream &input) const -> Geodata {
  Geodata geodata;

  for (auto x = 0; x < 256; ++x) {
    for (auto y = 0; y < 256; ++y) {
      std::uint8_t type = 0;
      input >> utils::extract<llvm::ulittle8_t>(type);

      if (type == BLOCK_SIMPLE) {
        std::int16_t z = 0;
        input >> utils::extract<llvm::little16_t>(z);

        geodata.blocks.push_back({
            static_cast<std::int16_t>(x * 8),
            static_cast<std::int16_t>(y * 8),
            z,
            static_cast<BlockType>(type),
            true,
            true,
            true,
            true,
        });
      } else if (type == BLOCK_COMPLEX) {
        for (auto cx = 0; cx < 8; ++cx) {
          for (auto cy = 0; cy < 8; ++cy) {
            geodata.blocks.push_back(
                extract_complex_block(input, type, x, y, cx, cy));
          }
        }
      } else if (type == BLOCK_MULTILAYER) {
        for (auto cx = 0; cx < 8; ++cx) {
          for (auto cy = 0; cy < 8; ++cy) {
            std::uint8_t layers = 0;
            input >> utils::extract<llvm::ulittle8_t>(layers);

            for (auto i = 0; i < layers; ++i) {
              geodata.blocks.push_back(
                  extract_complex_block(input, type, x, y, cx, cy));
            }
          }
        }
      }
    }
  }

  return geodata;
}

auto L2JSerializer::extract_complex_block(std::istream &input,
                                          std::uint8_t type, int x, int y,
                                          int cx, int cy) const -> Block {

  std::int16_t z = 0;
  input >> utils::extract<llvm::little16_t>(z);

  const std::uint8_t nswe = z & 0x000f;
  z = z & 0xfff0;
  z = z >> 1;

  return {
      static_cast<std::int16_t>(x * 8 + cx),
      static_cast<std::int16_t>(y * 8 + cy),
      z,
      static_cast<BlockType>(type),
      (nswe & 0x1) != 0,
      (nswe & 0x2) != 0,
      (nswe & 0x4) != 0,
      (nswe & 0x8) != 0,
  };
}

} // namespace geodata
