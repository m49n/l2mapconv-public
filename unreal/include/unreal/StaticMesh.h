#pragma once

#include "Archive.h"
#include "BSP.h"
#include "Index.h"
#include "Material.h"
#include "Object.h"
#include "ObjectRef.h"
#include "Primitives.h"
#include "Property.h"

#include <cstdint>
#include <vector>

namespace unreal {

struct StaticMeshSurface {
  std::uint32_t unknown;
  std::uint16_t first_index;
  std::uint16_t min_vertex_index;
  std::uint16_t max_vertex_index;
  std::uint16_t triangle_count;
  std::uint16_t triangle_max;

  friend auto operator>>(Archive &archive, StaticMeshSurface &surface)
      -> Archive &;
};

struct StaticMeshVertex {
  Vector location, normal;

  friend auto operator>>(Archive &archive, StaticMeshVertex &vertex)
      -> Archive &;
};

struct StaticMeshVertexStream {
  std::vector<StaticMeshVertex> vertices;
  std::uint32_t revision;

  friend auto operator>>(Archive &archive, StaticMeshVertexStream &stream)
      -> Archive &;
};

struct StaticMeshColorStream {
  std::vector<Color> colors;
  std::uint32_t revision;

  friend auto operator>>(Archive &archive, StaticMeshColorStream &stream)
      -> Archive &;
};

struct StaticMeshUV {
  float u, v;

  friend auto operator>>(Archive &archive, StaticMeshUV &uv) -> Archive &;
};

struct StaticMeshUVStream {
  std::vector<StaticMeshUV> uvs;
  std::uint32_t coordinate_index;
  std::uint32_t revision;

  friend auto operator>>(Archive &archive, StaticMeshUVStream &stream)
      -> Archive &;
};

struct StaticMeshIndexStream {
  std::vector<std::uint16_t> indices;
  std::uint32_t revision;

  friend auto operator>>(Archive &archive, StaticMeshIndexStream &stream)
      -> Archive &;
};

struct StaticMeshMaterial {
  ObjectRef<Material, ObjectRefConstraint::Optional> material;
  bool enable_collision;
};

class StaticMesh : public Primitive {
public:
  std::vector<StaticMeshSurface> surfaces;
  //  Box another_bb;
  StaticMeshVertexStream vertex_stream;
  StaticMeshColorStream color_stream, alpha_stream;
  std::vector<StaticMeshUVStream> uv_stream;
  StaticMeshIndexStream index_stream, wireframe_index_stream;
  ObjectRef<Model, ObjectRefConstraint::Optional> collision_model;

  std::vector<StaticMeshMaterial> materials;
  bool use_simple_line_collision;
  bool use_simple_box_collision;
  bool use_simple_karma_collision;

  explicit StaticMesh(Archive &archive)
      : Primitive{archive}, use_simple_line_collision{false},
        use_simple_box_collision{true}, use_simple_karma_collision{true} {}

  virtual void deserialize() override;
  virtual auto set_property(const Property &property) -> bool override;
};

} // namespace unreal
