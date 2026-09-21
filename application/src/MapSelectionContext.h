#pragma once

#include "MapLoadOptions.h"
#include "MapResidency.h"

#include <compare>
#include <map>
#include <optional>
#include <string>

enum class MapResidencyStatus {
  NotResident,
  Queued,
  Loading,
  Resident,
  Failed,
};

struct MapStatusKey {
  MapCoordinate coordinate;
  MapLayer layer;
  auto operator<=>(const MapStatusKey &) const = default;
};

struct MapSelectionSummary {
  std::size_t manual_selected{};
  std::size_t terrain_resident{};
  std::size_t detail_resident{};
  std::size_t queued{};
  std::size_t loading{};
  std::size_t failed{};
};

class MapSelectionContext {
public:
  explicit MapSelectionContext(MapCatalog catalog);

  auto catalog() const -> const MapCatalog &;
  auto manual_selection() const -> const Coordinates &;
  void set_manual(MapCoordinate coordinate, bool selected);
  void select_all_manual();
  void clear_manual();
  auto take_retry_requests() -> Coordinates;

  auto current() const -> std::optional<MapCoordinate>;
  void set_current(MapCoordinate coordinate);
  void clear_current();
  auto current_label() const -> std::string;

  auto auto_load() const -> bool;
  void set_auto_load(bool enabled);
  auto include_neighbors() const -> bool;
  void set_include_neighbors(bool enabled);

  void set_status(MapCoordinate coordinate, MapLayer layer,
                  MapResidencyStatus status, std::string failure = {});
  auto status(MapCoordinate coordinate, MapLayer layer) const
      -> MapResidencyStatus;
  auto failure(MapCoordinate coordinate, MapLayer layer) const -> std::string;
  auto summary() const -> MapSelectionSummary;
  auto residency_intent() const -> MapResidencyIntent;

private:
  MapCatalog m_catalog;
  Coordinates m_manual;
  Coordinates m_retry_requests;
  std::optional<MapCoordinate> m_current;
  bool m_auto_load{true};
  bool m_include_neighbors{true};
  std::map<MapStatusKey, MapResidencyStatus> m_statuses;
  std::map<MapStatusKey, std::string> m_failures;
};
