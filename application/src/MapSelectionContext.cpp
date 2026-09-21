#include "MapSelectionContext.h"

#include <set>
#include <utility>

MapSelectionContext::MapSelectionContext(MapCatalog catalog)
    : m_catalog{std::move(catalog)} {}

auto MapSelectionContext::catalog() const -> const MapCatalog & {
  return m_catalog;
}

auto MapSelectionContext::manual_selection() const -> const Coordinates & {
  return m_manual;
}

void MapSelectionContext::set_manual(MapCoordinate coordinate, bool selected) {
  if (!m_catalog.contains(coordinate)) {
    return;
  }
  if (selected) {
    m_manual.insert(coordinate);
  } else {
    m_manual.erase(coordinate);
  }
}

void MapSelectionContext::select_all_manual() {
  for (const auto &region : m_catalog.regions()) {
    m_manual.insert(region.coordinate);
  }
}

void MapSelectionContext::clear_manual() { m_manual.clear(); }

auto MapSelectionContext::current() const -> std::optional<MapCoordinate> {
  return m_current;
}

void MapSelectionContext::set_current(MapCoordinate coordinate) {
  m_current = coordinate;
}

void MapSelectionContext::clear_current() { m_current.reset(); }

auto MapSelectionContext::current_label() const -> std::string {
  if (!m_current) {
    return "outside catalog";
  }
  const auto *region = m_catalog.find(*m_current);
  return region == nullptr ? "outside catalog" : region->name;
}

auto MapSelectionContext::auto_load() const -> bool { return m_auto_load; }

void MapSelectionContext::set_auto_load(bool enabled) { m_auto_load = enabled; }

auto MapSelectionContext::include_neighbors() const -> bool {
  return m_include_neighbors;
}

void MapSelectionContext::set_include_neighbors(bool enabled) {
  m_include_neighbors = enabled;
}

void MapSelectionContext::set_status(MapCoordinate coordinate, MapLayer layer,
                                     MapResidencyStatus status,
                                     std::string failure) {
  const MapStatusKey key{coordinate, layer};
  if (status == MapResidencyStatus::NotResident) {
    m_statuses.erase(key);
  } else {
    m_statuses.insert_or_assign(key, status);
  }
  if (status == MapResidencyStatus::Failed) {
    m_failures.insert_or_assign(key, std::move(failure));
  } else {
    m_failures.erase(key);
  }
}

auto MapSelectionContext::status(MapCoordinate coordinate, MapLayer layer) const
    -> MapResidencyStatus {
  const auto found = m_statuses.find({coordinate, layer});
  return found == m_statuses.end() ? MapResidencyStatus::NotResident
                                   : found->second;
}

auto MapSelectionContext::failure(MapCoordinate coordinate,
                                  MapLayer layer) const -> std::string {
  const auto found = m_failures.find({coordinate, layer});
  return found == m_failures.end() ? std::string{} : found->second;
}

auto MapSelectionContext::summary() const -> MapSelectionSummary {
  MapSelectionSummary result{.manual_selected = m_manual.size()};
  std::set<MapCoordinate> queued;
  std::set<MapCoordinate> loading;
  std::set<MapCoordinate> failed;

  for (const auto &[key, value] : m_statuses) {
    if (value == MapResidencyStatus::Resident) {
      if (key.layer == MapLayer::Terrain) {
        ++result.terrain_resident;
      } else {
        ++result.detail_resident;
      }
    } else if (value == MapResidencyStatus::Queued) {
      queued.insert(key.coordinate);
    } else if (value == MapResidencyStatus::Loading) {
      loading.insert(key.coordinate);
    } else if (value == MapResidencyStatus::Failed) {
      failed.insert(key.coordinate);
    }
  }

  result.queued = queued.size();
  result.loading = loading.size();
  result.failed = failed.size();
  return result;
}

auto MapSelectionContext::residency_intent() const -> MapResidencyIntent {
  return {.manual = m_manual,
          .current = m_current,
          .auto_load = m_auto_load,
          .include_neighbors = m_include_neighbors};
}
