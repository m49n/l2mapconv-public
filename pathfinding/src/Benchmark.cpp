#include <algorithm>
#include <cmath>
#include <pathfinding/Benchmark.h>
#include <stdexcept>

namespace pathfinding {
void validate_benchmark(const BenchmarkSettings &s) {
  if (s.warmup < 0 || s.warmup > 10000 || s.iterations < 1 ||
      s.iterations > 1000)
    throw std::invalid_argument(
        "Benchmark needs warmup 0..10000 and measurements 1..1000");
}
Json benchmark_json(const BenchmarkSettings &s) {
  validate_benchmark(s);
  return {{"warmup", s.warmup}, {"iterations", s.iterations}};
}
BenchmarkSettings benchmark_from_json(const Json &j) {
  if (!j.at("warmup").is_number_integer() ||
      !j.at("iterations").is_number_integer() || j.at("warmup") < 0 ||
      j.at("warmup") > 10000 || j.at("iterations") < 1 ||
      j.at("iterations") > 1000)
    throw std::invalid_argument("Invalid benchmark counts");
  BenchmarkSettings s{j.at("warmup").get<int>(), j.at("iterations").get<int>()};
  validate_benchmark(s);
  return s;
}
Json summarize_benchmark(const Json &samples,
                         const BenchmarkSettings &settings) {
  validate_benchmark(settings);
  if (!samples.is_array() ||
      samples.size() != static_cast<std::size_t>(settings.iterations))
    throw std::invalid_argument(
        "Incomplete measured series (warmup must be excluded)");
  Json result = benchmark_json(settings), statuses = Json::object(),
       limits = Json::object();
  for (const auto &sample : samples) {
    const auto status = sample.at("status").get<std::string>();
    if (status.empty() || status.size() > 64)
      throw std::invalid_argument("Invalid sample status");
    statuses[status] = statuses.value(status, 0) + 1;
    if (sample.contains("limit_reason") &&
        !sample.at("limit_reason").is_null()) {
      const auto reason = sample.at("limit_reason").get<std::string>();
      if (reason.size() > 64)
        throw std::invalid_argument("Invalid sample limit reason");
      if (!reason.empty())
        limits[reason] = limits.value(reason, 0) + 1;
    }
    if (!sample.contains("total_ns") || sample.at("total_ns").is_null())
      throw std::invalid_argument("Missing total query time");
  }
  result["status_counts"] = statuses;
  result["limit_counts"] = limits;
  result["scope"] = "Same A->B, one JVM per backend; all measured outcomes "
                    "included; no result cache";
  result["percentiles"] = "Median averages middle pair; p95/p99 nearest rank";
  for (const auto *key : {"search_ns", "direct_ns", "snap_ns", "smoothing_ns",
                          "validation_ns", "total_ns"}) {
    std::vector<double> values;
    for (const auto &sample : samples) {
      const auto v = sample.value(key, Json(nullptr));
      if (v.is_null())
        continue;
      if (!v.is_number() || !std::isfinite(v.get<double>()) ||
          v.get<double>() < 0)
        throw std::invalid_argument("Invalid benchmark timer");
      values.push_back(v.get<double>());
    }
    Json stat{{"count", values.size()},
              {"median_ns", nullptr},
              {"p95_ns", nullptr},
              {"p99_ns", nullptr}};
    if (!values.empty()) {
      std::sort(values.begin(), values.end());
      const auto n = values.size();
      stat["median_ns"] = (values[(n - 1) / 2] + values[n / 2]) / 2;
      stat["p95_ns"] = values[static_cast<std::size_t>(std::ceil(.95 * n)) - 1];
      stat["p99_ns"] = values[static_cast<std::size_t>(std::ceil(.99 * n)) - 1];
      stat["min_ns"] = values.front();
      stat["max_ns"] = values.back();
    }
    result[key] = std::move(stat);
  }
  return result;
}
} // namespace pathfinding
