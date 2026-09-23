#include "pch.h"

#include "NSWE.h"

#define ENABLE_SIMPLE_NSWE_CALCULATION

namespace geodata {

void mark_walkable_triangles(float walkable_angle, const float *vertices,
                             const int *triangles, std::size_t triangle_count,
                             unsigned char *areas);

auto triangles_at_columns(const rcHeightfield &hf, const Map &map,
                          const std::vector<std::vector<int>> &triangle_index,
                          int x, int y, int radius = 0)
    -> std::vector<geometry::Triangle>;

inline auto allow_direction(int area, int direction) -> int {
  return area | 1 << (direction + 2);
}

inline auto forbid_direction(int area, int direction) -> int {
  return area & ~(1 << (direction + 2));
}

inline auto change_area(int area, int new_area) -> int {
  return new_area | unpack_nswe(area) << 2;
}

inline auto direction_forbidden(int area, int direction) -> bool {
  return (area >> 2 & (1 << direction)) == 0;
}

inline auto vertical_slope(const glm::vec3 &vector) -> float {
  // TODO: Vector can be already normalized
  return glm::dot(glm::normalize(vector), {0.0f, 1.0f, 0.0f});
}

template <typename T>
auto fill_vector(std::vector<std::vector<T>> &vector, int size) {
  vector.reserve(size);

  for (auto i = 0; i < size; ++i) {
    auto inner_vector = std::vector<T>();
    vector.push_back(std::move(inner_vector));
  }
}

NSWE::NSWE(const Map &map, float actor_height, float actor_radius,
           float max_walkable_angle, float min_walkable_climb,
           float max_walkable_climb, float cell_size, float cell_height)
    : m_map{map}, m_actor_height{actor_height}, m_actor_radius{actor_radius},
      m_max_walkable_angle_radians{std::cos(glm::radians(max_walkable_angle))},
      m_min_walkable_climb{min_walkable_climb},
      m_max_walkable_climb{max_walkable_climb}, m_cell_size{cell_size},
      m_cell_height{cell_height}, m_hf{rcAllocHeightfield()} {

  utils::Log(utils::LOG_INFO, "Geodata")
      << "Building intial heightfield" << std::endl;
  build_filtered_heightfield();
}

NSWE::~NSWE() { rcFreeHeightField(m_hf); }

auto NSWE::calculate_nswe() -> const rcHeightfield & {

#ifdef ENABLE_SIMPLE_NSWE_CALCULATION
  utils::Log(utils::LOG_INFO, "Geodata")
      << "Simple NSWE calculation" << std::endl;
  calculate_simple_nswe();
#endif

  utils::Log(utils::LOG_INFO, "Geodata") << "Collision detection" << std::endl;
  calculate_complex_nswe();

  return *m_hf;
}

void NSWE::build_filtered_heightfield() {
  const auto *bb_min = glm::value_ptr(m_map.internal_bounding_box().min());
  const auto *bb_max = glm::value_ptr(m_map.internal_bounding_box().max());

  // Grid size
  auto width = 0;
  auto height = 0;
  rcCalcGridSize(bb_min, bb_max, m_cell_size, &width, &height);

  // Create heightfield
  rcContext context{};
  rcCreateHeightfield(&context, *m_hf, width, height, bb_min, bb_max,
                      m_cell_size, m_cell_height);

  // Prepare geometry data
  const auto *vertices = glm::value_ptr(m_map.vertices().front());
  const auto vertex_count = m_map.vertices().size();
  const auto *triangles = reinterpret_cast<const int *>(m_map.indices().data());
  const auto triangle_count = m_map.indices().size() / 3;

  // Rasterize triangles
  fill_vector(m_triangle_index, width * height);
  fill_vector(m_triangle_cache, width * height);

  std::vector<unsigned char> areas(triangle_count);
  mark_walkable_triangles(vertices, triangles, triangle_count, &areas.front());
  rcRasterizeTriangles(&context, vertices, vertex_count, triangles,
                       &areas.front(), triangle_count, *m_hf,
                       &m_triangle_index.front());

  // Filter too short spans
  rcFilterWalkableLowHeightSpans(
      &context, static_cast<int>(m_actor_height / m_cell_height), *m_hf);
}

void NSWE::mark_walkable_triangles(const float *vertices, const int *triangles,
                                   std::size_t triangle_count,
                                   unsigned char *areas) const {

  for (std::size_t i = 0; i < triangle_count; ++i) {
    const auto *triangle = &triangles[i * 3];
    const auto normal =
        glm::triangleNormal(glm::make_vec3(&vertices[triangle[0] * 3]),
                            glm::make_vec3(&vertices[triangle[1] * 3]),
                            glm::make_vec3(&vertices[triangle[2] * 3]));

    const auto slope = vertical_slope(normal);

    if (slope < -0.01f) { // -0.01f workaround for "nearly vertical" surfaces
      areas[i] = RC_NULL_AREA;
    } else if (slope < m_max_walkable_angle_radians) {
      areas[i] = RC_STEEP_AREA;
    } else {
      areas[i] = RC_FLAT_AREA;
    }
  }
}

void NSWE::calculate_simple_nswe() {
  const auto actor_height_cells =
      static_cast<int>(m_actor_height / m_cell_height);
  const auto min_walkable_climb_cells =
      static_cast<int>(m_min_walkable_climb / m_cell_height);
  const auto max_walkable_climb_cells =
      static_cast<int>(m_max_walkable_climb / m_cell_height);

  const auto max_height = 0xffff;

  for (auto y = 0; y < m_hf->height; ++y) {
    for (auto x = 0; x < m_hf->width; ++x) {
      print_progress(x, y);

      for (auto *span = m_hf->spans[x + y * m_hf->width]; span != nullptr;
           span = span->next) {

        const auto area = unpack_area(span->area);

        if (area == RC_NULL_AREA) {
          continue;
        }

        const auto bottom = static_cast<int>(span->smax);
        const auto top = span->next != nullptr
                             ? static_cast<int>(span->next->smin)
                             : max_height;

        for (auto direction = 0; direction < 4; ++direction) {
          const auto side_x = x + rcGetDirOffsetX(direction);
          const auto side_y = y + rcGetDirOffsetY(direction);

          // Allow moving outside of the map
          if (side_x < 0 || side_y < 0 || side_x >= m_hf->width ||
              side_y >= m_hf->height) {

            span->area = allow_direction(span->area, direction);
            continue;
          }

          auto direction_allowed = false;

          for (auto *neighbour = m_hf->spans[side_x + side_y * m_hf->width];
               neighbour != nullptr; neighbour = neighbour->next) {

            // Builder discards these surfaces. They cannot be a landing layer,
            // even when their height would otherwise allow a step or a drop.
            const auto neighbour_area = unpack_area(neighbour->area);
            if (neighbour_area == RC_NULL_AREA) {
              continue;
            }

            const auto neighbour_bottom = static_cast<int>(neighbour->smax);
            const auto neighbour_top =
                neighbour->next != nullptr
                    ? static_cast<int>(neighbour->next->smin)
                    : max_height;

            const auto height = std::min(top, neighbour_top) -
                                std::max(bottom, neighbour_bottom);
            const auto diff = neighbour_bottom - bottom;

            if (height > actor_height_cells) {
              if (area <= RC_STEEP_AREA || neighbour_area <= RC_STEEP_AREA) {
                // Forbid going up on steep surfaces
                direction_allowed = diff <= min_walkable_climb_cells;
              } else {
                direction_allowed = diff <= max_walkable_climb_cells;

                // Mark complex areas for further sphere-to-mesh collision
                // detection
                if (std::abs(diff) >= min_walkable_climb_cells &&
                    std::abs(diff) <= max_walkable_climb_cells) {

                  span->area = change_area(span->area, RC_COMPLEX_AREA);
                }
              }

              break;
            }
          }

          if (direction_allowed) {
            span->area = allow_direction(span->area, direction);
          }
        }
      }
    }
  }
}

void NSWE::calculate_complex_nswe() {
  for (auto y = 0; y < m_hf->height; ++y) {
    for (auto x = 0; x < m_hf->width; ++x) {
      print_progress(x, y);

      for (auto *span = m_hf->spans[x + y * m_hf->width]; span != nullptr;
           span = span->next) {

#ifdef ENABLE_SIMPLE_NSWE_CALCULATION
        const auto area = unpack_area(span->area);

        if (area != RC_COMPLEX_AREA) {
          continue;
        }
#endif

        for (auto direction = 0; direction < 4; ++direction) {
#ifdef ENABLE_SIMPLE_NSWE_CALCULATION
          // Skip collision checking if direction is already forbidden at the
          // simple NSWE calculation step
          if (direction_forbidden(span->area, direction)) {
            continue;
          }
#endif

          const auto dx = rcGetDirOffsetX(direction);
          const auto dy = rcGetDirOffsetY(direction);
          const auto side_x = x + dx;
          const auto side_y = y + dy;

          // Skip map edges
          if (side_x < 0 || side_y < 0 || side_x >= m_hf->width ||
              side_y >= m_hf->height) {

            continue;
          }

          if (slide_sphere_until_collision(x, y, span->smax, direction)) {
            span->area = forbid_direction(span->area, direction);
          } else {
#ifndef ENABLE_SIMPLE_NSWE_CALCULATION
            span->area = allow_direction(span->area, direction);
#endif
          }
        }
      }
    }
  }
}

// TODO: Naive and very slow implementation
auto NSWE::slide_sphere_until_collision(int x, int y, int z,
                                        int direction) const -> bool {

  static constexpr auto delta = 1.0f;

  static const auto triangles_fetch_radius =
      static_cast<int>(std::ceil(m_actor_radius * 2.0f / m_cell_size));

  const auto map_origin = m_map.bounding_box().min();
  const auto sphere_radius = 16;

  const auto dx = rcGetDirOffsetX(direction);
  const auto dy = rcGetDirOffsetY(direction);

  // Place sphere on the cell
  const glm::vec3 sphere_center{
      map_origin.x + (x - dx * 0.5f) * m_cell_size + m_cell_size / 2.0f,
      map_origin.z + z * m_cell_height +
          sphere_radius * 2.0f, // Z-up swapped with Y-up
      map_origin.y + (y - dy * 0.5f) * m_cell_size + m_cell_size / 2.0f,
  };

  geometry::Sphere sphere{sphere_center, sphere_radius};

  const auto triangles = triangles_at_columns(x, y, triangles_fetch_radius);

  for (auto i = 0; i < static_cast<int>(m_cell_size * 1.5f / delta); ++i) {
    drop_sphere(sphere, triangles);

    sphere.center.x += dx * delta;
    sphere.center.z += dy * delta;

    for (const auto &triangle : triangles) {
      geometry::Intersection intersection{};

      if (sphere.intersects(triangle, intersection)) {
        const auto slope =
            vertical_slope(intersection.normal * intersection.depth);

        if (slope < 0.3f) {
          return true;
        }
      }
    }

    sphere.center.y += delta;
  }

  return false;
}

void NSWE::drop_sphere(geometry::Sphere &sphere,
                       const std::vector<geometry::Triangle> &triangles) const {

  static constexpr auto delta = 1.0f;

  const auto original_z = sphere.center.y;

  while (original_z - sphere.center.y < m_max_walkable_climb * 2.0f) {
    if (sphere.intersects(triangles)) {
      if (sphere.center.y != original_z) {
        sphere.center.y += delta;
      }

      return;
    }

    sphere.center.y -= delta;
  }
}

auto NSWE::triangles_at_columns(int x, int y, int radius) const
    -> std::vector<geometry::Triangle> {

  auto &triangles = m_triangle_cache[x + y * m_hf->width];

  if (triangles.empty()) {
    std::unordered_set<int> column_indices;

    const auto &map_vertices = m_map.vertices();
    const auto &map_triangles = m_map.indices();

    // Find triangle indices
    for (auto dy = y - radius; dy < y + radius + 1; ++dy) {
      for (auto dx = x - radius; dx < x + radius + 1; ++dx) {
        if (dx < 0 || dy < 0 || dx >= m_hf->width || dy >= m_hf->height) {
          continue;
        }

        const auto &indices = m_triangle_index[dx + dy * m_hf->width];
        column_indices.insert(indices.cbegin(), indices.cend());
      }
    }

    // Make triangles by found indices
    for (const auto index : column_indices) {
      const geometry::Triangle triangle{
          map_vertices[map_triangles[index * 3 + 0]],
          map_vertices[map_triangles[index * 3 + 1]],
          map_vertices[map_triangles[index * 3 + 2]],
      };

      triangles.push_back(triangle);
    }
  }

  return triangles;
}

void NSWE::print_progress(int x, int y) const {
  if (utils::Log::level < utils::LOG_INFO) {
    return;
  }

  const auto current = (x + 1) + (y + 1) * m_hf->width;
  const auto total = m_hf->height * m_hf->width;

  if (current == total) {
    std::cout << std::endl;
    return;
  }

  const auto part = total / 100;

  if (part == 0) {
    return;
  }

  if (current % part != 0) {
    return;
  }

  std::cout << ".";
}

} // namespace geodata
