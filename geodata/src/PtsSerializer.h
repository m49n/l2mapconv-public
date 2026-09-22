#pragma once

#include <geodata/ExportBuffer.h>

#include <cstdint>
#include <iosfwd>

namespace geodata {

class PtsSerializer {
public:
  void serialize(const ExportBuffer &buffer, std::uint8_t region_x,
                 std::uint8_t region_y, std::ostream &output) const;
};

} // namespace geodata
