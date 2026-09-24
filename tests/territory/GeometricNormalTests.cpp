#include "Fixtures.h"
#include "TestSupport.h"
#include <limits>
#include <territory/GeometricNormals.h>

int geometric_normal_tests() {
  using namespace territory;
  int failures = 0;
  VisualMesh mesh;
  for (auto p : {glm::vec3(0, 0, 7), glm::vec3(4, 0, -2), glm::vec3(0, 3, 5),
                 glm::vec3(0, 0, 2)}) {
    VisualVertex v;
    v.position = p;
    v.normal = {1, 0, 0};
    v.uv = {glm::vec2(1, 2), {3, 4}, {5, 6}, {7, 8}};
    v.color = {.1f, .2f, .3f, .4f};
    mesh.vertices.push_back(v);
  }
  // Select only the middle two triangles: a surviving plane and a collapsed
  // edge.
  mesh.indices = {3, 3, 3, 0, 1, 2, 0, 1, 3, 2, 2, 2};
  Draw d{0, 0, 3, 6, glm::mat4(1), false, "mixed triangles"};
  d.transform[2] = glm::vec4(0);
  d.transform[3] = {10000000, -10000000, 5, 1};
  auto result = rebuild_geometric_normals(mesh, d);
  failures += expect(
      result.mesh.indices == std::vector<std::uint32_t>{0, 1, 2} &&
          result.mesh.vertices.size() == 3 && result.skipped_triangles == 1,
      "draw range keeps its surface and drops only the collapsed triangle");
  if (result.mesh.vertices.size() == 3) {
    for (int i = 0; i < 3; ++i) {
      const auto &v = result.mesh.vertices[i];
      failures += expect(v.position == mesh.vertices[i].position &&
                             v.uv == mesh.vertices[i].uv &&
                             v.color == mesh.vertices[i].color &&
                             v.normal == glm::vec3(0, 0, 1),
                         "positions and all material attributes survive; "
                         "normals come from area");
    }
  }
  failures += expect(mesh.vertices[0].normal == glm::vec3(1, 0, 0) &&
                         mesh.indices.size() == 12,
                     "rebuilding one actor does not modify its shared mesh");
  for (int zero_axis : {0, 1, 2}) {
    d.transform = glm::mat4(0);
    d.transform[(zero_axis + 1) % 3] = {1.25f, -.5f, .75f, 0};
    d.transform[3] = {10, 20, 30, 1};
    result = rebuild_geometric_normals(mesh, d);
    failures +=
        expect(result.mesh.indices.empty() && result.skipped_triangles == 2,
               "rotated rank-one transforms contain no surface");
  }
  d.transform = glm::mat4(1);
  d.transform[2] = glm::vec4(0);
  d.transform[0][0] = -1;
  result = rebuild_geometric_normals(mesh, d);
  failures += expect(result.mesh.vertices[0].normal == glm::vec3(0, 0, -1),
                     "rank-two reflection uses transformed geometric winding");
  d.transform[0] = {1, 0, .25f, 0};
  d.transform[1] = {0, 1, .5f, 0};
  result = rebuild_geometric_normals(mesh, d);
  failures +=
      expect(glm::length(result.mesh.vertices[0].normal -
                         glm::normalize(glm::vec3(-.25f, -.5f, 1))) < 1e-6f,
             "flattened plane normals retain its world-space tilt");
  // No arbitrary area epsilon: tiny but real triangles must survive too.
  d.transform[0] = {1e-8f, 0, 0, 0};
  d.transform[1] = {0, 1e-8f, 0, 0};
  result = rebuild_geometric_normals(mesh, d);
  failures +=
      expect(result.mesh.indices.size() == 3 &&
                 result.mesh.vertices[0].normal == glm::vec3(0, 0, 1),
             "tiny nonzero area is normalized without an absolute cutoff");
  d.transform[2][2] = 1e-8f;
  result = rebuild_geometric_normals(mesh, d);
  failures +=
      expect(result.mesh.indices.size() == 6 && result.skipped_triangles == 0,
             "small invertible transforms are not confused with lost area");
  d.transform[0][0] = -1e-8f;
  result = rebuild_geometric_normals(mesh, d);
  failures += expect(
      glm::length(result.mesh.vertices[0].normal -
                  glm::normalize(glm::vec3(-27, 8, 12))) < 1e-6f,
      "negative tiny determinant retains inverse-transpose normal orientation");
  bool cancelled = false;
  try {
    rebuild_geometric_normals(mesh, d, [] { return true; });
  } catch (const Cancelled &) {
    cancelled = true;
  }
  failures += expect(cancelled,
                     "geometric normal rebuilding preserves cancellation type");
  d.transform[0][0] = std::numeric_limits<float>::infinity();
  failures += expect(throws([&] { rebuild_geometric_normals(mesh, d); }),
                     "non-finite transforms remain errors");
  d.transform = glm::mat4(1);
  d.index_count = 7;
  failures += expect(throws([&] { rebuild_geometric_normals(mesh, d); }),
                     "partial triangles remain errors");
  d.index_count = 300;
  failures += expect(throws([&] { rebuild_geometric_normals(mesh, d); }),
                     "out-of-range draws remain errors");
  d.index_count = 6;
  mesh.indices[3] = 999;
  failures += expect(throws([&] { rebuild_geometric_normals(mesh, d); }),
                     "out-of-range vertex indices remain errors");
  mesh.indices[3] = 0;
  mesh.vertices[0].position.z = std::numeric_limits<float>::quiet_NaN();
  failures += expect(throws([&] { rebuild_geometric_normals(mesh, d); }),
                     "non-finite positions remain errors");
  return failures;
}
