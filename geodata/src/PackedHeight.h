#pragma once

#include <geodata/Geodata.h>

#include <stdexcept>
#include <string>

namespace geodata {

// Validate only final complex/multilayer output. Before compression a cell
// may still become a simple block, whose raw height uses all 16 signed bits.
inline void validate_packed_height(const Cell &cell) {
  if (cell.z < -16384 || cell.z > 16376) {
    throw std::runtime_error{"Geodata height " + std::to_string(cell.z) +
                             " at cell " + std::to_string(cell.x) + "," +
                             std::to_string(cell.y) +
                             " is outside the packed range [-16384, 16376]"};
  }
}

} // namespace geodata
