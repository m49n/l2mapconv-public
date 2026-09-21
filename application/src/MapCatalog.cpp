#include "MapCatalog.h"

#include <algorithm>
#include <charconv>
#include <map>
#include <string_view>

namespace {

auto parse_component(std::string_view text) -> std::optional<int> {
  if (text.empty() || !std::all_of(text.begin(), text.end(), [](char value) {
        return value >= '0' && value <= '9';
      })) {
    return std::nullopt;
  }

  int value{};
  const auto [end, error] =
      std::from_chars(text.data(), text.data() + text.size(), value);
  if (error != std::errc{} || end != text.data() + text.size()) {
    return std::nullopt;
  }
  return value;
}

auto parse_coordinate(std::string_view name) -> std::optional<MapCoordinate> {
  const auto separator = name.find('_');
  if (separator == std::string_view::npos) {
    return std::nullopt;
  }

  const auto x = parse_component(name.substr(0, separator));
  const auto y = parse_component(name.substr(separator + 1));
  if (!x || !y) {
    return std::nullopt;
  }
  return MapCoordinate{*x, *y};
}

} // namespace

auto MapCatalog::discover(const std::filesystem::path &client_root)
    -> MapCatalog {
  MapCatalog catalog;
  std::map<MapCoordinate, MapRegion> regions;

  const auto maps_directory = client_root / "Maps";
  for (const auto &entry : std::filesystem::directory_iterator{maps_directory}) {
    if (!entry.is_regular_file() || entry.path().extension() != ".unr") {
      continue;
    }

    const auto name = entry.path().stem().string();
    const auto coordinate = parse_coordinate(name);
    if (!coordinate) {
      continue;
    }

    MapRegion region{*coordinate, name, entry.path()};
    const auto existing = regions.find(*coordinate);
    if (existing == regions.end() || region.name < existing->second.name) {
      regions.insert_or_assign(*coordinate, std::move(region));
    }
  }

  catalog.m_regions.reserve(regions.size());
  for (auto &[coordinate, region] : regions) {
    (void)coordinate;
    catalog.m_regions.push_back(std::move(region));
  }

  if (!catalog.m_regions.empty()) {
    catalog.m_extents = {catalog.m_regions.front().coordinate.x,
                         catalog.m_regions.front().coordinate.x,
                         catalog.m_regions.front().coordinate.y,
                         catalog.m_regions.front().coordinate.y};
    for (const auto &region : catalog.m_regions) {
      catalog.m_extents.min_x =
          std::min(catalog.m_extents.min_x, region.coordinate.x);
      catalog.m_extents.max_x =
          std::max(catalog.m_extents.max_x, region.coordinate.x);
      catalog.m_extents.min_y =
          std::min(catalog.m_extents.min_y, region.coordinate.y);
      catalog.m_extents.max_y =
          std::max(catalog.m_extents.max_y, region.coordinate.y);
    }
  }

  return catalog;
}

auto MapCatalog::regions() const -> const std::vector<MapRegion> & {
  return m_regions;
}

auto MapCatalog::extents() const -> MapExtents { return m_extents; }

auto MapCatalog::contains(MapCoordinate coordinate) const -> bool {
  return find(coordinate) != nullptr;
}

auto MapCatalog::find(MapCoordinate coordinate) const -> const MapRegion * {
  const auto region = std::lower_bound(
      m_regions.begin(), m_regions.end(), coordinate,
      [](const MapRegion &candidate, MapCoordinate value) {
        return candidate.coordinate < value;
      });
  if (region == m_regions.end() || region->coordinate != coordinate) {
    return nullptr;
  }
  return &*region;
}

auto MapCatalog::neighbors(MapCoordinate center, int radius) const
    -> std::vector<MapCoordinate> {
  std::vector<MapCoordinate> result;
  if (radius < 0) {
    return result;
  }

  for (auto x = center.x - radius; x <= center.x + radius; ++x) {
    for (auto y = center.y - radius; y <= center.y + radius; ++y) {
      const MapCoordinate coordinate{x, y};
      if (contains(coordinate)) {
        result.push_back(coordinate);
      }
    }
  }
  return result;
}
