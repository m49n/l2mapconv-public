#pragma once

#include <compare>
#include <filesystem>
#include <string>
#include <vector>

struct MapCoordinate {
  int x{};
  int y{};
  auto operator<=>(const MapCoordinate &) const = default;
};

struct MapRegion {
  MapCoordinate coordinate;
  std::string name;
  std::filesystem::path path;
};

struct MapExtents {
  int min_x{};
  int max_x{};
  int min_y{};
  int max_y{};
  auto operator<=>(const MapExtents &) const = default;
};

class MapCatalog {
public:
  static auto discover(const std::filesystem::path &client_root) -> MapCatalog;

  auto regions() const -> const std::vector<MapRegion> &;
  auto extents() const -> MapExtents;
  auto contains(MapCoordinate coordinate) const -> bool;
  auto find(MapCoordinate coordinate) const -> const MapRegion *;
  auto neighbors(MapCoordinate center, int radius) const
      -> std::vector<MapCoordinate>;

private:
  std::vector<MapRegion> m_regions;
  MapExtents m_extents{};
};
