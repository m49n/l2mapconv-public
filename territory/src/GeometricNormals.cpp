#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <territory/GeometricNormals.h>

namespace territory {
GeometricNormalMesh rebuild_geometric_normals(const VisualMesh &mesh,
                                              const Draw &draw,
                                              const Cancel &cancel) {
  check_cancel(cancel);
  auto invalid = [&] {
    return std::runtime_error("Invalid geometric-normal input: " + draw.source);
  };
  if (draw.first_index > mesh.indices.size() ||
      draw.index_count > mesh.indices.size() - draw.first_index ||
      draw.index_count % 3 ||
      draw.index_count > std::numeric_limits<std::uint32_t>::max())
    throw invalid();
  for (int c = 0; c < 4; ++c)
    for (int r = 0; r < 4; ++r)
      if (!std::isfinite(draw.transform[c][r]))
        throw invalid();
  const glm::dmat3 linear(draw.transform);
  // Transform oriented triangle area, without an inverse or division by det.
  // cof(A)*(e1 x e2) == (A*e1) x (A*e2), including singular A. Computing the
  // cofactor first also makes a rotated one-axis/zero-scale collapse exact.
  const glm::dmat3 cofactor(glm::cross(linear[1], linear[2]),
                            glm::cross(linear[2], linear[0]),
                            glm::cross(linear[0], linear[1]));
  const bool mirrored = glm::determinant(linear) < 0;
  GeometricNormalMesh result;
  for (std::size_t i = draw.first_index;
       i < draw.first_index + draw.index_count; i += 3) {
    check_cancel(cancel);
    std::array<VisualVertex, 3> vertices;
    for (std::size_t j = 0; j < 3; ++j) {
      const auto index = mesh.indices[i + j];
      if (index >= mesh.vertices.size())
        throw invalid();
      vertices[j] = mesh.vertices[index];
      for (int axis = 0; axis < 3; ++axis)
        if (!std::isfinite(vertices[j].position[axis]))
          throw invalid();
    }
    const auto a = glm::dvec3(vertices[0].position);
    auto area = cofactor * glm::cross(glm::dvec3(vertices[1].position) - a,
                                      glm::dvec3(vertices[2].position) - a);
    const auto largest =
        std::max({std::abs(area.x), std::abs(area.y), std::abs(area.z)});
    if (!std::isfinite(largest))
      throw invalid();
    if (largest == 0) {
      ++result.skipped_triangles;
      continue;
    }
    // Normalize in double with rescaling, so tiny real surfaces are not lost.
    area /= largest;
    auto normal = glm::vec3(area / glm::length(area));
    // Match the renderer's front-face reversal for negative determinants.
    // At rank two det=0: the transformed triangle order defines orientation.
    if (mirrored)
      normal = -normal;
    for (auto &vertex : vertices) {
      vertex.normal = normal;
      result.mesh.indices.push_back(
          static_cast<std::uint32_t>(result.mesh.vertices.size()));
      result.mesh.vertices.push_back(vertex);
    }
  }
  return result;
}
} // namespace territory
