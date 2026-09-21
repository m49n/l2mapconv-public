#include "TestSupport.h"
#include "UnrealConverters.h"

#include <vector>

auto main() -> int {
  auto failures = 0;

  failures += expect(true, "test harness accepts successes");
  failures += expect(primary_uv({}, 0) == glm::vec2{0.0f, 0.0f},
                     "missing UV stream uses zero UV");

  unreal::StaticMeshUVStream stream{};
  stream.uvs.push_back({0.25f, 0.75f});
  const std::vector streams{stream};

  failures += expect(primary_uv(streams, 0) == glm::vec2{0.25f, 0.75f},
                     "primary UV is preserved");
  failures += expect(primary_uv(streams, 1) == glm::vec2{0.0f, 0.0f},
                     "short UV stream uses zero UV");

  return failures;
}
