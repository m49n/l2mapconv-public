#include <array>
#include <cmath>
#include <pathfinding/Result.h>
#include <stdexcept>
namespace pathfinding {
namespace {
constexpr std::array names{"reached",
                           "partial",
                           "no_path",
                           "unknown_legacy",
                           "invalid_endpoint",
                           "layer_mismatch",
                           "resource_limit",
                           "cancelled",
                           "invalid_data",
                           "backend_unavailable",
                           "failed",
                           "unsupported_scope"};
constexpr std::size_t max_points = 1000000;
void require(bool value, const char *what) {
  if (!value)
    throw std::invalid_argument(what);
}
std::string text(const Json &j, std::size_t limit = 4096) {
  auto s = j.get<std::string>();
  require(s.size() <= limit, "Result text exceeds limit");
  return s;
}
void validate_metadata(const Json &j, int depth = 0) {
  require(depth <= 16, "Result metadata too deeply nested");
  if (j.is_number_float())
    require(std::isfinite(j.get<double>()), "Nonfinite metric");
  if (j.is_string())
    (void)text(j);
  if (j.is_structured()) {
    require(j.size() <= max_points, "Metadata exceeds limit");
    for (const auto &v : j)
      validate_metadata(v, depth + 1);
  }
}
std::optional<ResolvedEndpoint> endpoint(const Json &j) {
  if (j.is_null())
    return std::nullopt;
  return ResolvedEndpoint{
      point_from_json(j.at("requested")), point_from_json(j.at("resolved")),
      j.at("valid").get<bool>(), text(j.at("surface_id"), 256)};
}
Json endpoint(const std::optional<ResolvedEndpoint> &p) {
  if (!p)
    return nullptr;
  return {{"requested", point_json(p->requested)},
          {"resolved", point_json(p->resolved)},
          {"valid", p->valid},
          {"surface_id", p->surface_id}};
}
std::vector<WorldPoint> points(const Json &j) {
  require(j.is_array() && j.size() <= max_points, "Path exceeds point limit");
  std::vector<WorldPoint> out;
  out.reserve(j.size());
  for (const auto &p : j)
    out.push_back(point_from_json(p));
  return out;
}
Json points(const std::vector<WorldPoint> &v) {
  require(v.size() <= max_points, "Path exceeds point limit");
  auto j = Json::array();
  for (const auto &p : v)
    j.push_back(point_json(p));
  return j;
}
} // namespace
const char *status_name(RouteStatus s) {
  const auto i = static_cast<std::size_t>(s);
  if (i >= names.size())
    throw std::invalid_argument("Unknown route status");
  return names[i];
}
Json result_json(const RouteResult &r) {
  auto corridor = Json::array();
  std::size_t total = 0;
  require(r.corridor.size() <= max_points, "Too many corridor polygons");
  for (const auto &p : r.corridor) {
    total += p.size();
    require(total <= max_points, "Corridor exceeds point limit");
    corridor.push_back(points(p));
  }
  Json j{{"schema", 1},
         {"request_id", r.request_id},
         {"case_id", r.case_id},
         {"backend", r.backend},
         {"status", status_name(r.status)},
         {"a", endpoint(r.a)},
         {"b", endpoint(r.b)},
         {"raw", points(r.raw)},
         {"final_path", points(r.final_path)},
         {"validated_samples", points(r.validated_samples)},
         {"corridor", corridor},
         {"segments_valid",
          r.segments_valid ? Json(*r.segments_valid) : Json(nullptr)},
         {"metrics", r.metrics},
         {"identity", r.identity},
         {"diagnostic", r.diagnostic}};
  (void)result_from_json(j);
  return j;
}
RouteResult result_from_json(const Json &j) {
  require(j.at("schema").is_number_integer() && j.at("schema") == 1,
          "Unsupported result schema");
  RouteResult r;
  r.request_id = text(j.at("request_id"), 128);
  r.case_id = text(j.at("case_id"), 128);
  r.backend = text(j.at("backend"), 128);
  require(!r.request_id.empty() && !r.case_id.empty() && !r.backend.empty(),
          "Result identity required");
  const auto name = text(j.at("status"));
  auto found = std::find(names.begin(), names.end(), name);
  require(found != names.end(), "Unknown route status");
  r.status = static_cast<RouteStatus>(found - names.begin());
  r.a = endpoint(j.at("a"));
  r.b = endpoint(j.at("b"));
  r.raw = points(j.at("raw"));
  r.final_path = points(j.at("final_path"));
  r.validated_samples = points(j.at("validated_samples"));
  const auto &corridor = j.at("corridor");
  require(corridor.is_array() && corridor.size() <= max_points,
          "Invalid corridor");
  std::size_t count = 0;
  for (const auto &polygon : corridor) {
    count += polygon.size();
    require(count <= max_points, "Corridor exceeds point limit");
    r.corridor.push_back(points(polygon));
  }
  if (!j.at("segments_valid").is_null())
    r.segments_valid = j.at("segments_valid").get<bool>();
  r.metrics = j.at("metrics");
  r.identity = j.at("identity");
  require(r.metrics.is_object() && r.identity.is_object(),
          "Result metrics and identity must be objects");
  validate_metadata(r.metrics);
  validate_metadata(r.identity);
  r.diagnostic = text(j.at("diagnostic"));
  return r;
}
} // namespace pathfinding
