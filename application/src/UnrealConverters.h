#pragma once

#include <unreal/Primitives.h>
#include <unreal/StaticMesh.h>

#include <geometry/Box.h>

#include <glm/glm.hpp>

inline auto to_vec3(const unreal::Vector &vector) -> glm::vec3 {
  return {vector.x, vector.y, vector.z};
}

inline auto to_box(const unreal::Box &box) -> geometry::Box {
  return geometry::Box{to_vec3(box.min), to_vec3(box.max)};
}

inline auto primary_uv(const std::vector<unreal::StaticMeshUVStream> &streams,
                       std::size_t vertex_index) -> glm::vec2 {
  if (streams.empty() || vertex_index >= streams.front().uvs.size()) {
    return {0.0f, 0.0f};
  }

  const auto &uv = streams.front().uvs[vertex_index];
  return {uv.u, uv.v};
}
