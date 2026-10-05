#include <charconv>
#include <cmath>
#include <pathfinding/BackendAdapters.h>
#include <pathfinding/Benchmark.h>
#include <set>
#include <sstream>
#include <stdexcept>
namespace pathfinding {
std::vector<std::string> visited_navigation_regions(const std::vector<WorldPoint>& points,const std::vector<std::string>& regions) {
  std::vector<std::string> visited;
  std::vector<std::pair<std::string,WorldPoint>> bounds;
  for(const auto& region:regions)bounds.emplace_back(region,region_origin(region));
  auto record=[&](WorldPoint p){
    validate_point(p);
    std::string owner;
    // Prefer half-open ownership; a final surface sample may lie exactly on
    // an outer polygon edge, for which closed bounds remain legitimate.
    for(bool closed:{false,true}) {
      for(const auto& [name,o]:bounds)
        if(p.x>=o.x && p.y>=o.y && (closed?p.x<=o.x+32768:p.x<o.x+32768) && (closed?p.y<=o.y+32768:p.y<o.y+32768)){owner=name;break;}
      if(!owner.empty())break;
    }
    if(owner.empty())throw std::invalid_argument("Route crosses an unloaded navigation region");
    if(visited.empty() || visited.back()!=owner)visited.push_back(owner);
  };
  if(points.size()==1)record(points.front());
  for(std::size_t i=1;i<points.size();++i) {
    const auto a=points[i-1],b=points[i];validate_point(a);validate_point(b);
    std::vector<double> cuts{0,1};
    for(const auto [start,end]:{std::pair{a.x,b.x},std::pair{a.y,b.y}})
      if(start!=end)for(double edge=(std::floor(std::min(start,end)/32768)+1)*32768;edge<std::max(start,end);edge+=32768)
        cuts.push_back((edge-start)/(end-start));
    std::sort(cuts.begin(),cuts.end());cuts.erase(std::unique(cuts.begin(),cuts.end()),cuts.end());
    for(std::size_t k=1;k<cuts.size();++k){const double t=(cuts[k-1]+cuts[k])/2;record({a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t});}
  }
  return visited;
}
namespace {
void require(bool condition, const char *message) {
  if (!condition)
    throw std::invalid_argument(message);
}
WorldPoint rounded(WorldPoint p) {
  return {std::round(p.x), std::round(p.y), std::round(p.z)};
}
bool same_floor(double actual, double confirmed) {
  return std::abs(actual - confirmed) <= 4;
}
double xy_distance(WorldPoint a, WorldPoint b) {
  return std::hypot(a.x - b.x, a.y - b.y);
}
std::vector<WorldPoint> decode_path(const Json &j) {
  if (j.is_null())
    return {};
  auto bytes = j.get<std::string>();
  std::vector<WorldPoint> points;
  if (bytes.empty())
    return points;
  std::size_t begin = 0;
  for (;;) {
    auto end = bytes.find('|', begin);
    if (end == std::string::npos)
      end = bytes.size();
    const char *p = bytes.data() + begin, *last = bytes.data() + end;
    double numbers[3]{};
    for (int i = 0; i < 3; ++i) {
      const auto value = std::from_chars(p, last, numbers[i]);
      require(value.ec == std::errc{}, "Malformed legacy path coordinate");
      p = value.ptr;
      if (i < 2) {
        require(p < last && *p == ',', "Malformed legacy path point");
        ++p;
      }
    }
    require(p == last, "Trailing legacy path coordinate data");
    WorldPoint point{numbers[0], numbers[1], numbers[2]};
    validate_point(point);
    points.push_back(point);
    require(points.size() <= 1000000, "Legacy route exceeds point limit");
    if (end == bytes.size())
      break;
    begin = end + 1;
  }
  return points;
}
} // namespace
std::string legacy_case_csv(const RouteCase &c, int first_x, int first_y) {
  validate_case(c);
  require(first_x >= 0 && first_x <= 99 && first_y >= 0 && first_y <= 99,
          "Invalid server geo origin");
  std::ostringstream s;
  s << "case_id,route_name,mode,from_label,from_x,from_y,from_z,from_heading,"
       "from_geo_x,from_geo_y,from_ref,to_label,to_x,to_y,to_z,to_heading,to_"
       "geo_x,to_geo_y,to_ref,coordinate_source\n";
  s << c.id << ',' << c.id << ",mouse_click,A,";
  auto emit = [&](WorldPoint p) {
    p = rounded(p);
    s << static_cast<int>(p.x) << ',' << static_cast<int>(p.y) << ','
      << static_cast<int>(p.z) << ",0,"
      << static_cast<int>(std::floor((p.x - (first_x - 20) * 32768.0) / 16))
      << ','
      << static_cast<int>(std::floor((p.y - (first_y - 18) * 32768.0) / 16))
      << ",0";
  };
  emit(c.a.requested);
  s << ",B,";
  emit(c.b.requested);
  s << ",editor\n";
  return s.str();
}
RouteResult parse_legacy_jsonl(std::string_view bytes, const RouteCase &c,
                               std::string id) {
  validate_case(c);
  require(bytes.size() <= 16 * 1024 * 1024, "Legacy JSONL exceeds limit");
  Json run, sample, evidence, summary;
  auto samples=Json::array(), summaries=Json::array();
  std::istringstream input{std::string(bytes)};
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line == "\r")
      continue;
    if (line.starts_with("@GEOJSON@"))
      line.erase(0, 9);
    auto j = Json::parse(line);
    const auto type = j.at("type").get<std::string>();
    if (type == "run") {
      require(run.is_null(), "Multiple legacy runs");
      run = std::move(j);
      continue;
    }
    if (type != "sample" && type != "evidence" && type != "summary")
      continue;
    require(j.at("caseId") == c.id && j.at("fork") == 0,
            "Legacy evidence case/fork mismatch");
    if (type=="sample") {
      require(samples.size()<1000 && j.at("iteration").is_number_integer() &&
              j.at("iteration")==samples.size(), "Missing or duplicate legacy sample iteration");
      samples.push_back(std::move(j));
      continue;
    }
    if (type=="summary") { summaries.push_back(std::move(j)); continue; }
    Json *target = type == "sample"     ? &sample
                   : type == "evidence" ? &evidence
                                        : &summary;
    require(target->is_null(),
            "Multiple legacy records for one interactive request");
    *target = std::move(j);
  }
  require(!run.is_null() && !samples.empty() && !evidence.is_null() &&
              !summaries.empty(),
          "Incomplete legacy evidence");
  const auto counts=benchmark_from_json({{"warmup",run.value("warmup",Json(0))},
                                        {"iterations",run.value("iterations",Json(1))}});
  require(samples.size()==static_cast<std::size_t>(counts.iterations),"Incomplete legacy measurement series");
  sample=samples.back();summary=summaries.front();
  std::set<std::string> summary_statuses;
  for(const auto& s:summaries) {
    require(summary_statuses.insert(s.at("status").get<std::string>()).second,
            "Duplicate legacy status summary");
    require(s.value("legacyStartHeight",Json(nullptr))==summary.value("legacyStartHeight",Json(nullptr)) &&
            s.value("legacyTargetHeight",Json(nullptr))==summary.value("legacyTargetHeight",Json(nullptr)),
            "Legacy endpoint heights changed during series");
  }
  auto hashes = run.at("dataHashes").get<std::string>();
  std::transform(
      hashes.begin(), hashes.end(), hashes.begin(),
      [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  require(hashes == "{" + c.map + ".l2j=" + c.l2j.sha256 + "}",
          "Legacy loaded data identity mismatch");
  for (const auto& measured:samples) for (const auto &[prefix, requested] :
       std::vector<std::pair<std::string, WorldPoint>>{{"from", c.a.requested},
                                                       {"to", c.b.requested}}) {
    const auto p = rounded(requested);
    require(point_from_json(
                Json::array({measured.at(prefix + "X"), measured.at(prefix + "Y"),
                             measured.at(prefix + "Z")})) == p,
            "Legacy submitted coordinates mismatch");
  }
  RouteResult r;
  r.request_id = std::move(id);
  r.case_id = c.id;
  r.backend = "l2j";
  r.raw = decode_path(evidence.at("rawWaypoints"));
  r.final_path = decode_path(evidence.at("cleanedWaypoints"));
  r.validated_samples = decode_path(evidence.at("validatedCells"));
  if (!evidence.at("segmentsValid").is_null())
    r.segments_valid = evidence.at("segmentsValid").get<bool>();
  for (const auto &[source, destination] :
       std::vector<std::pair<std::string, std::string>>{
           {"directNs", "direct_ns"},
           {"astarNs", "search_ns"},
           {"cleanNs", "smoothing_ns"},
           {"validateNs", "validation_ns"},
           {"totalNs", "total_ns"},
           {"allocatedBytes", "allocated_bytes"},
           {"gcCountDelta", "gc_count"},
           {"gcTimeMsDelta", "gc_time_ms"}})
    r.metrics[destination] = sample.at(source);
  r.metrics["load_ns"] = run.at("loadNs");
  if (run.contains("warmup") && run.contains("iterations")) {
    auto series=Json::array();
    for(const auto& s:samples) {
      const auto status=s.at("status").get<std::string>();
      require(status=="DIRECT_VALID" || status=="ASTAR_VALID" || status=="DIRECT_INVALID" ||
              status=="ASTAR_INVALID" || status=="UNKNOWN_LEGACY","Unknown legacy sample status");
      series.push_back({{"status",status},{"search_ns",s.at("astarNs")},
        {"direct_ns",s.at("directNs")},{"smoothing_ns",s.at("cleanNs")},
        {"validation_ns",s.at("validateNs")},{"total_ns",s.at("totalNs")},
        {"limit_reason",s.value("limitReason",Json(nullptr))}});
    }
    r.metrics["benchmark_warmup"]=counts.warmup;
    r.metrics["benchmark_samples"]=series;
  }
  r.metrics["visited_nodes"] = evidence.at("visitedNodes");
  r.metrics["budget_consumed"] = evidence.at("budgetConsumed");
  // Counters from the timed sample must not be substituted by the separate
  // untimed geometry/evidence replay.
  for (const auto &[source, destination] :
       {std::pair{"visitedNodes", "visited_nodes"},
        {"searchTimeoutMs", "search_timeout_ms"},
        {"maxWindowCells", "max_window_cells"},
        {"peakWindowCells", "peak_window_cells"},
        {"limitReason", "limit_reason"},
        {"stopReason", "stop_reason"}})
    if (sample.contains(source))
      r.metrics[destination] = sample.at(source);
  r.identity = {
      {"run", run},
      {"submitted_a", point_json(rounded(c.a.requested))},
      {"submitted_b", point_json(rounded(c.b.requested))},
      {"endpoint_evidence",
       "GeoEngine.getHeight; no actor-specific shift or client execution"}};
  for (auto [key, source, out] :
       std::vector<std::tuple<const char *, Endpoint,
                              std::optional<ResolvedEndpoint> *>>{
           {"legacyStartHeight", c.a, &r.a},
           {"legacyTargetHeight", c.b, &r.b}}) {
    if (summary.contains(key) && !summary[key].is_null()) {
      auto p = rounded(source.requested);
      p.z = summary[key].get<double>();
      validate_point(p);
      *out = ResolvedEndpoint{source.requested, p, true, "l2j:getHeight"};
    }
  }
  const auto status = sample.at("status").get<std::string>();
  if (status == "UNKNOWN_LEGACY") {
    r.status = RouteStatus::UnknownLegacy;
    r.diagnostic = "Legacy null has no reliable failure reason";
  } else if (status == "DIRECT_INVALID" || status == "ASTAR_INVALID") {
    r.status =
        r.final_path.empty() ? RouteStatus::Failed : RouteStatus::Partial;
    r.diagnostic = "Legacy plan failed endpoint/segment validation";
  } else if (status == "DIRECT_VALID" || status == "ASTAR_VALID") {
    if (!r.a || !r.b) {
      r.status = RouteStatus::UnknownLegacy;
      r.diagnostic = "Legacy selected endpoint height unavailable";
    } else if (!same_floor(r.a->resolved.z, c.a.confirmed_z) ||
               !same_floor(r.b->resolved.z, c.b.confirmed_z)) {
      r.status = RouteStatus::LayerMismatch;
      r.diagnostic = "GeoEngine selected a different floor";
    } else {
      const auto end = point_from_json(Json::array(
          {sample.at("endX"), sample.at("endY"), sample.at("endZ")}));
      const bool good = r.segments_valid.value_or(false) &&
                        evidence.at("serverEndpointValid") == true &&
                        xy_distance(end, rounded(c.b.requested)) <= 16 &&
                        end.z == r.b->resolved.z && !r.final_path.empty() &&
                        !r.validated_samples.empty() &&
                        r.validated_samples.back() == end;
      r.status = good ? RouteStatus::Reached : RouteStatus::Failed;
      r.identity["observed_end"] = point_json(end);
      if (!good)
        r.diagnostic = "Legacy success contradicted by route evidence";
    }
  } else
    throw std::invalid_argument("Unknown legacy status");
  const auto limit = sample.value("limitReason", std::string{});
  require(limit.empty() || limit == "search_timeout" || limit == "window_limit",
          "Unknown legacy resource limit reason");
  if (!limit.empty()) {
    r.status = RouteStatus::ResourceLimit;
    r.diagnostic = "Timed L2J search stopped: " + limit;
  } else if (status == "UNKNOWN_LEGACY" &&
             sample.value("stopReason", std::string{}) == "exhausted") {
    r.status = RouteStatus::NoPath;
    r.diagnostic = "Timed L2J search exhausted the reachable open list";
  }
  if (r.status == RouteStatus::ResourceLimit ||
      r.status == RouteStatus::NoPath) {
    // The evidence pass may finish differently after JVM warm-up. Keep that
    // evidence in legacy.jsonl, not as a misleading route for the timed run.
    r.raw.clear();
    r.final_path.clear();
    r.validated_samples.clear();
    r.segments_valid.reset();
  }
  (void)result_json(r);
  return r;
}
RouteResult parse_navmesh_json(const Json &j, const RouteCase &c,
                               std::string id) {
  validate_case(c);
  auto r = result_from_json(j);
  require(r.request_id == id && r.case_id == c.id && r.backend == "navmesh",
          "Navmesh result identity mismatch");
  bool invalid_endpoint = false;
  bool floor_mismatch = false;
  for (const auto &[resolved, endpoint] :
       std::vector<std::pair<std::optional<ResolvedEndpoint>, Endpoint>>{
           {r.a, c.a}, {r.b, c.b}}) {
    if (!resolved)
      continue;
    require(resolved->requested == endpoint.requested,
            "Navmesh source endpoint changed");
    if (!resolved->valid ||
        xy_distance(resolved->resolved, endpoint.requested) >
            c.snap_horizontal ||
        std::abs(resolved->resolved.z - endpoint.requested.z) >
            c.snap_vertical) {
      invalid_endpoint = true;
    }
    floor_mismatch |= !same_floor(resolved->resolved.z, endpoint.confirmed_z);
  }
  // Validate both source identities before classifying either endpoint.
  // Endpoint warnings must not hide a failed search or surface validation.
  const bool reached = r.status == RouteStatus::Reached;
  const auto warn = [&](const char *message) {
    if (!r.diagnostic.empty())
      r.diagnostic += "; ";
    r.diagnostic += message;
  };
  if (invalid_endpoint) {
    if (reached)
      r.status = RouteStatus::InvalidEndpoint;
    warn("Nearest polygon exceeds endpoint snap limits");
  }
  if (floor_mismatch) {
    // An invalid snap takes priority when normalizing a claimed success.
    if (reached && !invalid_endpoint)
      r.status = RouteStatus::LayerMismatch;
    warn("Navmesh selected a different floor");
  }
  if (r.status == RouteStatus::Reached) {
    require(r.a && r.b, "Reached result lacks resolved endpoints");
    require(r.segments_valid.value_or(false) && !r.final_path.empty() &&
                xy_distance(r.final_path.back(), r.b->resolved) < 0.1 &&
                std::abs(r.final_path.back().z - r.b->resolved.z) < 0.1,
            "Reached result lacks valid final surface path");
  }
  return r;
}
} // namespace pathfinding
