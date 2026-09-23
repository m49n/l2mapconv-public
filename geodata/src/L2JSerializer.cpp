#include "pch.h"

#include "Compressor.h"
#include "L2JSerializer.h"
#include "PackedHeight.h"

namespace geodata {

static constexpr auto MAP_WIDTH_BLOCKS = 256;
static constexpr auto MAP_HEIGHT_BLOCKS = 256;
static constexpr auto BLOCK_WIDTH_CELLS = 8;
static constexpr auto BLOCK_HEIGHT_CELLS = 8;

auto L2JSerializer::deserialize(std::istream &input) const -> Geodata {
  Geodata geodata;

  for (auto x = 0; x < MAP_WIDTH_BLOCKS; ++x) {
    for (auto y = 0; y < MAP_HEIGHT_BLOCKS; ++y) {
      std::uint8_t type = 0;
      input >> utils::extract<llvm::ulittle8_t>(type);

      if (type == BLOCK_SIMPLE) {
        std::int16_t z = 0;
        input >> utils::extract<llvm::little16_t>(z);

        geodata.cells.push_back({
            static_cast<std::int16_t>(x * BLOCK_WIDTH_CELLS),
            static_cast<std::int16_t>(y * BLOCK_HEIGHT_CELLS),
            z,
            static_cast<BlockType>(type),
            true,
            true,
            true,
            true,
        });
      } else if (type == BLOCK_COMPLEX) {
        for (auto cx = 0; cx < BLOCK_WIDTH_CELLS; ++cx) {
          for (auto cy = 0; cy < BLOCK_HEIGHT_CELLS; ++cy) {
            geodata.cells.push_back(
                read_complex_block_cell(input, type, x, y, cx, cy));
          }
        }
      } else if (type == BLOCK_MULTILAYER) {
        for (auto cx = 0; cx < BLOCK_WIDTH_CELLS; ++cx) {
          for (auto cy = 0; cy < BLOCK_HEIGHT_CELLS; ++cy) {
            std::uint8_t layers = 0;
            input >> utils::extract<llvm::ulittle8_t>(layers);

            for (auto i = 0; i < layers; ++i) {
              geodata.cells.push_back(
                  read_complex_block_cell(input, type, x, y, cx, cy));
            }
          }
        }
      } else {
        ASSERT(false, "Geodata",
               "Invalid block type: " << static_cast<int>(type));
      }
    }
  }

  return geodata;
}

void L2JSerializer::serialize(const ExportBuffer &buffer,
                              std::ostream &output) const {

  for (auto x = 0; x < MAP_WIDTH_BLOCKS; ++x) {
    for (auto y = 0; y < MAP_HEIGHT_BLOCKS; ++y) {
      const auto &block = buffer.block(x, y);

      output.put(block.type);

      if (block.type == BLOCK_SIMPLE) {
        const auto cell = buffer.cell(x, y);
        std::int16_t z = cell.z;
        write(output, z);
      } else if (block.type == BLOCK_COMPLEX) {
        for (auto cx = 0; cx < BLOCK_WIDTH_CELLS; ++cx) {
          for (auto cy = 0; cy < BLOCK_HEIGHT_CELLS; ++cy) {
            const auto cell = buffer.cell(x, y, cx, cy);
            write_complex_block_cell(output, cell);
          }
        }
      } else if (block.type == BLOCK_MULTILAYER) {
        for (auto cx = 0; cx < BLOCK_WIDTH_CELLS; ++cx) {
          for (auto cy = 0; cy < BLOCK_HEIGHT_CELLS; ++cy) {
            const auto column = buffer.column(x, y, cx, cy);

            output.put(column.layers);

            for (auto layer = 0; layer < column.layers; ++layer) {
              const auto cell = buffer.cell(x, y, cx, cy, layer);
              write_complex_block_cell(output, cell);
            }
          }
        }
      } else {
        ASSERT(false, "Geodata",
               "Invalid block type: " << static_cast<int>(block.type));
      }
    }
  }
}

auto L2JSerializer::read_complex_block_cell(std::istream &input,
                                            std::uint8_t type, int x, int y,
                                            int cx, int cy) const -> Cell {

  std::int16_t z = 0;
  input >> utils::extract<llvm::little16_t>(z);

  const std::uint8_t nswe = z & 0x000f;
  z = z & 0xfff0;
  z = z >> 1;

  return {
      static_cast<std::int16_t>(x * BLOCK_WIDTH_CELLS + cx),
      static_cast<std::int16_t>(y * BLOCK_HEIGHT_CELLS + cy),
      z,
      static_cast<BlockType>(type),
      (nswe & DIRECTION_N) != 0,
      (nswe & DIRECTION_S) != 0,
      (nswe & DIRECTION_W) != 0,
      (nswe & DIRECTION_E) != 0,
  };
}

void L2JSerializer::write_complex_block_cell(std::ostream &output,
                                             const Cell &cell) const {

  validate_packed_height(cell);

  // Calculate NSWE
  const std::uint8_t nswe =
      (cell.north ? DIRECTION_N : 0) | (cell.south ? DIRECTION_S : 0) |
      (cell.west ? DIRECTION_W : 0) | (cell.east ? DIRECTION_E : 0);

  std::int16_t z = cell.z;
  z = (z << 1) | nswe; // add NSWE

  write(output, z);
}

void L2JSerializer::write(std::ostream &output, std::int16_t value) const {
  value = (value & 0x00ff) | (value & 0xff00); // swap bytes
  output.write(reinterpret_cast<char *>(&value), sizeof(value));
};

} // namespace geodata
