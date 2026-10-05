#pragma once
#include "Result.h"
#include <geodata/Geodata.h>
#include <navmesh/Navmesh.h>

namespace pathfinding {
struct VisibleCell {
  WorldPoint minimum;
  double width{};
  unsigned nswe{};
};
struct OverviewCell {
  WorldPoint minimum;
  double width{};
  unsigned nswe{};
  bool mixed{}, detailed{};
};
class Dataset {
public:
  static Dataset load(std::string map, const FileIdentity &l2j,
                      const FileIdentity &nav);
  static Dataset load(std::string map, const FileIdentity &l2j,
                      const std::vector<NavRegionInput>&);
  const std::vector<NavRegionInput>& nav_regions() const { return m_nav_regions; }
  glm::dvec4 game_bounds() const;
  std::optional<std::string> region_at(double x,double y) const;
  auto candidates(double x, double y) const -> std::vector<ResolvedEndpoint>;
  auto nav_polygons(double min_z, double max_z) const
      -> std::vector<std::vector<WorldPoint>>;
  auto l2j_cells(glm::dvec4 xy_bounds, double min_z, double max_z,
                 std::size_t max_cells) const -> std::vector<VisibleCell>;
  // Covers every occupied bin in the requested area; never returns a prefix.
  // Coarse bins summarize all layers in the slice, not a traversable cell.
  auto l2j_overview(glm::dvec4 xy_bounds, double min_z, double max_z,
                    unsigned cell_width) const -> std::vector<OverviewCell>;
  bool contains(double x, double y) const;
  const std::string &map() const { return m_map; }
  WorldPoint origin() const { return m_origin; }
  std::uint64_t load_ns() const { return m_load_ns; }
  const FileIdentity &l2j_identity() const { return m_l2j; }
  const FileIdentity &nav_identity() const { return m_nav; }

private:
  struct Span {
    std::size_t begin{}, end{};
  };
  std::string m_map;
  WorldPoint m_origin;
  FileIdentity m_l2j, m_nav;
  std::vector<NavRegionInput> m_nav_regions;
  std::vector<std::string> m_regions;
  geodata::Geodata m_geo;
  std::vector<Span> m_blocks;
  navmesh::Mesh m_mesh;
  std::vector<std::vector<WorldPoint>> m_polygons;
  std::uint64_t m_load_ns{};
};
} // namespace pathfinding
