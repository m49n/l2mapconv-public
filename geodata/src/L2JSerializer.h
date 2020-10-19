#pragma once

#include <geodata/Geodata.h>

#include <iostream>

namespace geodata {

class L2JSerializer {
public:
  auto deserialize(std::istream &input) const -> Geodata;
  //  void serialize(const Geodata &geodata, std::ostream &output) const;

private:
  auto extract_complex_block(std::istream &input, std::uint8_t type, int x,
                             int y, int cx, int cy) const -> Block;
};

} // namespace geodata
