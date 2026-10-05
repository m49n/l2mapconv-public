#pragma once
#include "Case.h"
#include <optional>

namespace pathfinding {
struct BenchmarkSettings {
  int warmup{2000}, iterations{100};
};
void validate_benchmark(const BenchmarkSettings &);
Json benchmark_json(const BenchmarkSettings &);
BenchmarkSettings benchmark_from_json(const Json &);
// Input contains only measured samples, never warmup. Timings are nanoseconds.
Json summarize_benchmark(const Json &samples, const BenchmarkSettings &);
} // namespace pathfinding
