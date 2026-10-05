#pragma once
#include "Case.h"
#include <optional>

namespace pathfinding {
enum class RouteStatus {
  Reached,
  Partial,
  NoPath,
  UnknownLegacy,
  InvalidEndpoint,
  LayerMismatch,
  ResourceLimit,
  Cancelled,
  InvalidData,
  BackendUnavailable,
  Failed,
  UnsupportedScope
};
struct ResolvedEndpoint {
  WorldPoint requested, resolved;
  bool valid{};
  std::string surface_id;
};
struct RouteResult {
  std::string request_id, case_id, backend;
  RouteStatus status{RouteStatus::Failed};
  std::optional<ResolvedEndpoint> a, b;
  std::vector<WorldPoint> raw, final_path, validated_samples;
  std::vector<std::vector<WorldPoint>> corridor;
  std::optional<bool> segments_valid;
  Json metrics = Json::object(), identity = Json::object();
  std::string diagnostic;
};
const char *status_name(RouteStatus);
Json result_json(const RouteResult &);
RouteResult result_from_json(const Json &);
} // namespace pathfinding
