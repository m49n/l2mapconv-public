#pragma once

#include "Map.h"

#include <cstddef>
#include <cstdint>
#include <iostream>

struct GeometryClassStats {
  std::size_t actors{};
  std::size_t vertices{};
  std::size_t triangles{};
};

struct MapGeometryStats {
  GeometryClassStats terrain{};
  GeometryClassStats static_meshes{};
  GeometryClassStats csg{};
  GeometryClassStats blocking_volumes{};
};

namespace scene_stats_detail {

inline auto class_stats(MapGeometryStats &stats, std::uint64_t type)
    -> GeometryClassStats * {
  if ((type & SURFACE_TERRAIN) != 0) {
    return &stats.terrain;
  }
  if ((type & SURFACE_STATIC_MESH) != 0) {
    return &stats.static_meshes;
  }
  if ((type & SURFACE_CSG) != 0) {
    return &stats.csg;
  }
  if ((type & SURFACE_BLOCKING_VOLUME) != 0) {
    return &stats.blocking_volumes;
  }
  return nullptr;
}

} // namespace scene_stats_detail

inline auto map_geometry_stats(const Map &map) -> MapGeometryStats {
  MapGeometryStats result{};

  for (const auto &entity : map.entities) {
    if (entity.mesh == nullptr) {
      continue;
    }

    const auto instances = entity.mesh->instance_matrices.empty()
                               ? std::size_t{1}
                               : entity.mesh->instance_matrices.size();
    GeometryClassStats *seen[4]{};
    auto seen_count = std::size_t{};

    for (const auto &surface : entity.mesh->surfaces) {
      if ((surface.type & SURFACE_BOUNDING_BOX) != 0) {
        continue;
      }

      auto *const stats = scene_stats_detail::class_stats(result, surface.type);
      if (stats == nullptr) {
        continue;
      }

      stats->triangles += (surface.index_count / 3) * instances;

      auto already_seen = false;
      for (auto i = std::size_t{}; i < seen_count; ++i) {
        already_seen = already_seen || seen[i] == stats;
      }
      if (!already_seen) {
        seen[seen_count++] = stats;
        stats->actors += instances;
        stats->vertices += entity.mesh->vertices.size() * instances;
      }
    }
  }

  return result;
}

inline auto has_required_preview_geometry(const MapGeometryStats &stats)
    -> bool {
  const auto present = [](const GeometryClassStats &value) {
    return value.actors > 0 && value.vertices > 0 && value.triangles > 0;
  };

  return present(stats.terrain) && present(stats.static_meshes) &&
         present(stats.csg) && present(stats.blocking_volumes);
}

inline auto map_contains_only(const Map &map, std::uint64_t allowed_types)
    -> bool {
  constexpr auto base_types = SURFACE_TERRAIN | SURFACE_STATIC_MESH |
                              SURFACE_CSG | SURFACE_IMPORTED_GEODATA |
                              SURFACE_GENERATED_GEODATA |
                              SURFACE_BLOCKING_VOLUME;
  for (const auto &entity : map.entities) {
    if (entity.mesh == nullptr) {
      return false;
    }
    for (const auto &surface : entity.mesh->surfaces) {
      const auto surface_base = surface.type & base_types;
      if (surface_base == 0 || (surface_base & ~allowed_types) != 0) {
        return false;
      }
    }
  }
  return true;
}

inline void print_scene_stats(const Map &map,
                              std::ostream &output = std::cout) {
  const auto stats = map_geometry_stats(map);
  const auto print = [&output](const char *name,
                               const GeometryClassStats &value) {
    output << name << " actors=" << value.actors
           << " vertices=" << value.vertices
           << " triangles=" << value.triangles << '\n';
  };
  print("terrain", stats.terrain);
  print("static_mesh", stats.static_meshes);
  print("csg", stats.csg);
  print("blocking_volume", stats.blocking_volumes);
}
