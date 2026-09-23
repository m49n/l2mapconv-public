#include "pch.h"

#include "Compressor.h"
#include "NSWE.h"

#include <geodata/Builder.h>

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace geodata {

auto Builder::build(const Map &map, const BuilderSettings &settings) const
    -> const ExportBuffer & {

  NSWE nswe_calculator{
      map,
      settings.actor_height,
      settings.actor_radius,
      settings.max_walkable_angle,
      settings.min_walkable_climb,
      settings.max_walkable_climb,
      settings.cell_size,
      settings.cell_height,
  };

  const auto &hf = nswe_calculator.calculate_nswe();

  // Convert heightfield to geodata
  Geodata geodata;

  const auto map_origin = map.bounding_box().min();

#ifdef GEODATA_POST_PROCESSING
  const auto cell_elevation = map_origin.z + settings.actor_height / 2.0f;
#else
  const auto cell_elevation = map_origin.z + settings.cell_height;
#endif

  std::vector<int> columns(hf.width * hf.height);
  auto black_holes = 0;

  for (auto y = 0; y < hf.height; ++y) {
    for (auto x = 0; x < hf.width; ++x) {
      for (auto *span = hf.spans[x + y * hf.width]; span != nullptr;
           span = span->next) {

        const auto area = unpack_area(span->area);
        const auto nswe = unpack_nswe(span->area);

        if (area == RC_NULL_AREA) {
          continue;
        }

        if (nswe == 0) {
          black_holes++;
        }

        const auto height = cell_elevation + span->smax * settings.cell_height;
        if (!std::isfinite(height) ||
            height < std::numeric_limits<std::int16_t>::min() ||
            height > std::numeric_limits<std::int16_t>::max()) {
          throw std::runtime_error{"Geodata height " + std::to_string(height) +
                                   " in map " + map.name() + " at cell " +
                                   std::to_string(x) + "," + std::to_string(y) +
                                   " cannot be represented as int16"};
        }

        geodata.cells.push_back({
            static_cast<std::int16_t>(x), //
            static_cast<std::int16_t>(y), //
            static_cast<std::int16_t>(height),                            //
            BLOCK_MULTILAYER,                                             //
            (nswe & DIRECTION_N) != 0,                                    //
            (nswe & DIRECTION_W) != 0,                                    //
            (nswe & DIRECTION_E) != 0,                                    //
            (nswe & DIRECTION_S) != 0,                                    //
                                       //            area == RC_COMPLEX_AREA,
                                       //            area == RC_COMPLEX_AREA,
                                       //            area == RC_COMPLEX_AREA,
                                       //            area == RC_COMPLEX_AREA,
        });

        columns[y * hf.width + x]++;
      }
    }
  }

  // Add fake cells to columns with no layers
  for (auto y = 0; y < hf.height; ++y) {
    for (auto x = 0; x < hf.width; ++x) {
      if (columns[y * hf.width + x] > 0) {
        continue;
      }

      geodata.cells.push_back({
          static_cast<std::int16_t>(x),
          static_cast<std::int16_t>(y),
          -0x4000,
          BLOCK_COMPLEX,
          false,
          false,
          false,
          false,
      });
    }
  }

    utils::Log(utils::LOG_INFO, "Geodata")
        << "Map Proccessed:" << map.name() << " - Black holes (points of no return): " << black_holes << std::endl;

  // Compress export buffer and return it
  m_export_buffer.reset(geodata);

#ifdef GEODATA_POST_PROCESSING
  Compressor compressor{m_export_buffer};
  compressor.compress();
#endif

  return m_export_buffer;
}

} // namespace geodata
