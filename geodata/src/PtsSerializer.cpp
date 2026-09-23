#include "pch.h"

#include "PtsSerializer.h"
#include "PackedHeight.h"

#include <stdexcept>

namespace geodata {
namespace {

constexpr auto REGION_WIDTH_BLOCKS = 256;
constexpr auto BLOCK_WIDTH_CELLS = 8;

void write_u16(std::ostream &output, std::uint16_t value) {
  output.put(static_cast<char>(value & 0xff));
  output.put(static_cast<char>((value >> 8) & 0xff));
}

void write_u32(std::ostream &output, std::uint32_t value) {
  write_u16(output, static_cast<std::uint16_t>(value));
  write_u16(output, static_cast<std::uint16_t>(value >> 16));
}

void write_cell(std::ostream &output, const Cell &cell) {
  validate_packed_height(cell);
  const auto nswe = static_cast<std::uint16_t>(
      (cell.north ? DIRECTION_N : 0) | (cell.south ? DIRECTION_S : 0) |
      (cell.west ? DIRECTION_W : 0) | (cell.east ? DIRECTION_E : 0));
  const auto height = static_cast<std::uint16_t>(cell.z);
  write_u16(output,
            static_cast<std::uint16_t>(((height << 1) & 0xfff0) | nswe));
}

auto layer_count(const ExportBuffer &buffer, int x, int y) -> std::uint16_t {
  auto total = 0;
  for (auto cx = 0; cx < BLOCK_WIDTH_CELLS; ++cx) {
    for (auto cy = 0; cy < BLOCK_WIDTH_CELLS; ++cy) {
      const auto layers = buffer.column(x, y, cx, cy).layers;
      if (layers == 0) {
        throw std::runtime_error{"PTS multilayer block has an empty column"};
      }
      total += layers;
    }
  }
  return static_cast<std::uint16_t>(total);
}

} // namespace

void PtsSerializer::serialize(const ExportBuffer &buffer, std::uint8_t region_x,
                              std::uint8_t region_y,
                              std::ostream &output) const {
  auto flat_blocks = 0U;
  auto complex_blocks = 0U;
  auto serialized_cells = 0U;

  for (auto x = 0; x < REGION_WIDTH_BLOCKS; ++x) {
    for (auto y = 0; y < REGION_WIDTH_BLOCKS; ++y) {
      switch (buffer.block(x, y).type) {
      case BLOCK_SIMPLE:
        ++flat_blocks;
        break;
      case BLOCK_COMPLEX:
        ++complex_blocks;
        serialized_cells += 64;
        break;
      case BLOCK_MULTILAYER: {
        const auto layers = layer_count(buffer, x, y);
        serialized_cells += layers;
        if (layers == 64) {
          ++complex_blocks;
        }
        break;
      }
      default:
        throw std::runtime_error{
            "PTS export encountered an unknown block type"};
      }
    }
  }

  output.put(static_cast<char>(region_x));
  output.put(static_cast<char>(region_y));
  write_u16(output, 128);
  write_u16(output, 16);
  write_u32(output, serialized_cells);
  write_u32(output, flat_blocks + complex_blocks);
  write_u32(output, flat_blocks);

  for (auto x = 0; x < REGION_WIDTH_BLOCKS; ++x) {
    for (auto y = 0; y < REGION_WIDTH_BLOCKS; ++y) {
      const auto type = buffer.block(x, y).type;
      if (type == BLOCK_SIMPLE) {
        write_u16(output, 0);
        const auto height = static_cast<std::uint16_t>(buffer.cell(x, y).z);
        write_u16(output, height);
        write_u16(output, height);
      } else if (type == BLOCK_COMPLEX || (type == BLOCK_MULTILAYER &&
                                           layer_count(buffer, x, y) == 64)) {
        write_u16(output, 0x40);
        for (auto cx = 0; cx < BLOCK_WIDTH_CELLS; ++cx) {
          for (auto cy = 0; cy < BLOCK_WIDTH_CELLS; ++cy) {
            write_cell(output, buffer.cell(x, y, cx, cy));
          }
        }
      } else if (type == BLOCK_MULTILAYER) {
        write_u16(output, layer_count(buffer, x, y));
        for (auto cx = 0; cx < BLOCK_WIDTH_CELLS; ++cx) {
          for (auto cy = 0; cy < BLOCK_WIDTH_CELLS; ++cy) {
            const auto layers = buffer.column(x, y, cx, cy).layers;
            write_u16(output, layers);
            for (auto layer = 0; layer < layers; ++layer) {
              write_cell(output, buffer.cell(x, y, cx, cy, layer));
            }
          }
        }
      } else {
        throw std::runtime_error{
            "PTS export encountered an unknown block type"};
      }
    }
  }

  if (!output) {
    throw std::runtime_error{"Failed to write PTS geodata"};
  }
}

} // namespace geodata
