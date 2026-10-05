#pragma once
namespace navmesh {
struct Settings {
  float actor_height{48}, actor_radius{16}, max_climb{16}, max_slope{45.5f};
  float cell_size{16}, cell_height{1};
  int tile_cells{64};
};
void validate(const Settings &);
} // namespace navmesh
