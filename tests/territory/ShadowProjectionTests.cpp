#include "Fixtures.h"
#include "TestSupport.h"
#include <array>
#include <cmath>
#include <glm/glm.hpp>
#include <territory/ShadowProjection.h>

int shadow_projection_tests() {
  using namespace territory;
  int failures = 0;
  const Bounds bounds{98304, 0, 131072, 32768, -100, 12000};
  const auto north = make_shadow_projection(bounds, 0, 40, 4096);
  const auto east = make_shadow_projection(bounds, 90, 40, 4096);
  const auto northwest = make_shadow_projection(bounds, 315, 40, 4096);
  failures += expect(std::abs(north.surface_to_sun.x) < 1e-5f &&
                         north.surface_to_sun.y < 0 &&
                         north.surface_to_sun.z > 0,
                     "azimuth zero places sun north of scene");
  failures += expect(east.surface_to_sun.x > 0 &&
                         std::abs(east.surface_to_sun.y) < 1e-5f,
                     "azimuth ninety places sun east of scene");
  failures += expect(northwest.surface_to_sun.x < 0 &&
                         northwest.surface_to_sun.y < 0 &&
                         northwest.surface_to_sun.z > 0 &&
                         northwest.size == 4096,
                     "315 degree sun is northwest and above scene");
  for (double x : {0.0, bounds.max_x - bounds.min_x})
    for (double y : {0.0, bounds.max_y - bounds.min_y})
      for (double z : {bounds.min_z, bounds.max_z}) {
        const auto clip = northwest.relative_world_to_clip *
                          glm::vec4(x, y, z, 1);
        const auto normalized = glm::vec3(clip) / clip.w;
        failures += expect(std::abs(normalized.x) < 1.001f &&
                               std::abs(normalized.y) < 1.001f &&
                               std::abs(normalized.z) < 1.001f,
                           "all scene corners fit the shadow projection");
      }
  auto translated = bounds;
  translated.min_x += 500000;
  translated.max_x += 500000;
  translated.min_y -= 120000;
  translated.max_y -= 120000;
  const auto moved = make_shadow_projection(translated, 315, 40, 4096);
  bool invariant = true;
  for (int column = 0; column < 4; ++column)
    for (int row = 0; row < 4; ++row)
      invariant &= std::abs(northwest.relative_world_to_clip[column][row] -
                            moved.relative_world_to_clip[column][row]) <
                   1e-5f;
  failures += expect(invariant,
                     "shadow projection ignores absolute map origin");
  const Bounds small{65536, 131072, 65664, 131200, -1, 25};
  const Bounds exterior{65516, 131052, 65664, 131200, -1, 25};
  const auto expanded = make_shadow_projection(small, exterior, 315, 40, 256);
  const auto clipped = make_shadow_projection(small, 315, 40, 256);
  const auto point = glm::vec4(-20, -20, 24, 1);
  const auto fits = expanded.relative_world_to_clip * point;
  const auto outside = clipped.relative_world_to_clip * point;
  failures += expect(std::abs(fits.x / fits.w) <= 1 &&
                         std::abs(fits.y / fits.w) <= 1 &&
                         std::abs(fits.z / fits.w) <= 1 &&
                         std::abs(outside.z / outside.w) > 1,
                     "exterior caster fits expanded light volume, not square-only volume");
  failures += expect(throws([&] {
                       (void)make_shadow_projection(bounds, 315, 40, 0);
                     }),
                     "shadow projection rejects zero resolution");
  return failures;
}
