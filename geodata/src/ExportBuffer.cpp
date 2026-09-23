#include "pch.h"

#include <geodata/ExportBuffer.h>

#include <stdexcept>
#include <string>

namespace geodata {

static constexpr auto MAP_WIDTH_BLOCKS = 256;
static constexpr auto MAP_HEIGHT_BLOCKS = 256;
static constexpr auto BLOCK_WIDTH_CELLS = 8;
static constexpr auto BLOCK_HEIGHT_CELLS = 8;
static constexpr auto MAP_WIDTH_CELLS = MAP_WIDTH_BLOCKS * BLOCK_WIDTH_CELLS;
static constexpr auto MAP_HEIGHT_CELLS = MAP_HEIGHT_BLOCKS * BLOCK_HEIGHT_CELLS;
static constexpr auto MAX_LAYERS = 64;
static constexpr auto CELL_HEIGHT = 8;

ExportBuffer::ExportBuffer()
    : m_blocks{MAP_WIDTH_BLOCKS * MAP_HEIGHT_BLOCKS},
      m_columns{MAP_WIDTH_CELLS * MAP_HEIGHT_CELLS}, m_cells{MAP_WIDTH_CELLS *
                                                             MAP_HEIGHT_CELLS *
                                                             MAX_LAYERS} {}

void ExportBuffer::reset(const Geodata &geodata) {
  std::fill(m_blocks.begin(), m_blocks.end(), Block{});
  std::fill(m_columns.begin(), m_columns.end(), Column{});
  std::fill(m_cells.begin(), m_cells.end(), PackedCell{});

  // Sort cells by Z for correct order of the layers
  auto sorted_cells = geodata.cells;
  std::sort(sorted_cells.begin(), sorted_cells.end(),
            [](const auto &a, const auto &b) { return a.z < b.z; });

  for (const auto &cell : sorted_cells) {
    const auto column_index = cell.y + cell.x * MAP_WIDTH_CELLS;
    const auto block_index = cell.y / BLOCK_HEIGHT_CELLS +
                             cell.x / BLOCK_WIDTH_CELLS * MAP_WIDTH_BLOCKS;

    auto &column = m_columns[column_index];
    auto &block = m_blocks[block_index];

    if (column.layers >= MAX_LAYERS) {
      throw std::runtime_error{"Too many geodata layers at cell " +
                               std::to_string(cell.x) + "," +
                               std::to_string(cell.y) + " (maximum 64)"};
    }
    const auto packed = pack_cell(cell);
    block.type = cell.type;
    column.layers++;
    m_cells[column_index * MAX_LAYERS + column.layers - 1] = packed;
  }
}

auto ExportBuffer::convert_to_geodata() const -> Geodata {
  Geodata geodata{};

  for (auto x = 0; x < MAP_WIDTH_BLOCKS; ++x) {
    for (auto y = 0; y < MAP_HEIGHT_BLOCKS; ++y) {
      const auto &block = this->block(x, y);

      if (block.type == BLOCK_SIMPLE) {
        const auto cell = this->cell(x, y);
        geodata.cells.emplace_back(cell);
      } else if (block.type == BLOCK_COMPLEX) {
        for (auto cx = 0; cx < BLOCK_WIDTH_CELLS; ++cx) {
          for (auto cy = 0; cy < BLOCK_HEIGHT_CELLS; ++cy) {
            const auto cell = this->cell(x, y, cx, cy);
            geodata.cells.emplace_back(cell);
          }
        }
      } else if (block.type == BLOCK_MULTILAYER) {
        for (auto cx = 0; cx < BLOCK_WIDTH_CELLS; ++cx) {
          for (auto cy = 0; cy < BLOCK_HEIGHT_CELLS; ++cy) {
            const auto &column = this->column(x, y, cx, cy);

            for (auto layer = 0; layer < column.layers; ++layer) {
              const auto cell = this->cell(x, y, cx, cy, layer);
              geodata.cells.emplace_back(cell);
            }
          }
        }
      } else {
        ASSERT(false, "Geodata",
               "Invalid block type: " << static_cast<int>(block.type));
      }
    }
  }

  return geodata;
}

auto ExportBuffer::block(int x, int y) const -> const Block & {
  const auto block_index = y + x * MAP_WIDTH_BLOCKS;
  return m_blocks[block_index];
}

auto ExportBuffer::column(int x, int y, int cx, int cy) const
    -> const Column & {

  const auto column_index = (y * BLOCK_HEIGHT_CELLS) + cy +
                            ((x * BLOCK_WIDTH_CELLS) + cx) * MAP_WIDTH_CELLS;
  return m_columns[column_index];
}

auto ExportBuffer::cell(int x, int y, int cx, int cy, int layer) const -> Cell {
  const auto column_y = (y * BLOCK_HEIGHT_CELLS) + cy;
  const auto column_x = (x * BLOCK_WIDTH_CELLS) + cx;
  const auto column_index = column_y + column_x * MAP_WIDTH_CELLS;
  const auto cell_index = column_index * MAX_LAYERS + layer;
  const auto block_index = y + x * MAP_WIDTH_BLOCKS;
  return unpack_cell(m_cells[cell_index], m_blocks[block_index].type, column_x,
                     column_y);
}

void ExportBuffer::set_block_type(int x, int y, BlockType type) {
  const auto block_index = y + x * MAP_WIDTH_BLOCKS;
  m_blocks[block_index].type = type;
}

void ExportBuffer::set_block_height(int x, int y, std::int16_t height) {
  const auto column_index =
      (y * BLOCK_HEIGHT_CELLS) + (x * BLOCK_WIDTH_CELLS) * MAP_WIDTH_CELLS;
  const auto cell_index = column_index * MAX_LAYERS;
  m_cells[cell_index].height = round_height(height);
}

auto ExportBuffer::pack_cell(const Cell &cell) const -> PackedCell {
  PackedCell packed_cell{};
  packed_cell.height = round_height(cell.z);
  packed_cell.north = cell.north;
  packed_cell.south = cell.south;
  packed_cell.west = cell.west;
  packed_cell.east = cell.east;
  return packed_cell;
}

auto ExportBuffer::unpack_cell(PackedCell packed_cell, BlockType type, int x,
                               int y) const -> Cell {

  Cell cell{};
  cell.x = x;
  cell.y = y;
  cell.z = packed_cell.height;
  cell.type = type;
  cell.north = packed_cell.north;
  cell.south = packed_cell.south;
  cell.west = packed_cell.west;
  cell.east = packed_cell.east;
  return cell;
}

auto ExportBuffer::round_height(std::int16_t height) const -> std::int16_t {
#ifdef GEODATA_POST_PROCESSING
  // Round cell height to fit 12 bits
  return (height / CELL_HEIGHT) * CELL_HEIGHT;
#else
  return height;
#endif
}

} // namespace geodata
