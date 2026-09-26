#include "Entity.h"
#include "TestSupport.h"

#include <rendering/DrawableMesh.h>
#include <rendering/Entity.h>
#include <rendering/EntityShader.h>
#include <rendering/Scene.h>

#include <cstddef>
#include <memory>
#include <vector>

namespace {

class FakeDrawableMesh final : public rendering::DrawableMesh {
public:
  explicit FakeDrawableMesh(std::uint64_t surface_type)
      : m_surfaces{rendering::MeshSurface{
            surface_type, rendering::Material{{}, nullptr}, 0, 3}},
        m_bounds{{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}} {}

  auto surfaces() const
      -> const std::vector<rendering::MeshSurface> & override {
    return m_surfaces;
  }

  auto bounding_box() const -> const geometry::Box & override {
    return m_bounds;
  }

  void draw(const rendering::MeshSurface &) const override {}

private:
  std::vector<rendering::MeshSurface> m_surfaces;
  geometry::Box m_bounds;
};

auto fake_shader_handle() -> std::shared_ptr<rendering::EntityShader> {
  auto *storage = ::operator new(sizeof(rendering::EntityShader));
  return {static_cast<rendering::EntityShader *>(storage),
          [](rendering::EntityShader *value) { ::operator delete(value); }};
}

auto make_entity(std::uint64_t surface_type) -> rendering::Entity {
  return rendering::Entity{std::make_shared<FakeDrawableMesh>(surface_type),
                           fake_shader_handle(), glm::mat4{1.0f}, false};
}

} // namespace

auto run_scene_group_tests() -> int {
  auto failures = 0;
  rendering::Scene scene;
  const auto first = scene.create_group();
  const auto second = scene.create_group();
  const auto first_entity = make_entity(SURFACE_TERRAIN);
  const auto second_entity = make_entity(SURFACE_STATIC_MESH);

  scene.add(first, first_entity);
  scene.add(second, second_entity);
  scene.set_group_visible(first, false);
  failures += expect(!scene.group_visible(first) && scene.group_visible(second) &&
                         scene.group_size(first) == 1,
                     "hiding one map keeps its entities resident and leaves neighbors visible");
  scene.set_group_visible(first, true);
  failures += expect(scene.group_visible(first),
                     "hidden map can be restored without reupload");
  failures +=
      expect(scene.group_size(first) == 1 && scene.group_size(second) == 1,
             "scene tracks entities by group");

  scene.remove_group(first);
  failures += expect(!scene.group_visible(first),
                     "removal clears map visibility state");
  failures +=
      expect(scene.group_size(first) == 0 && scene.group_size(second) == 1,
             "removing one map leaves another map intact");
  failures += expect(scene.tree_entity_count() == 1,
                     "render tree drops only removed group entries");

  scene.add(second, make_entity(SURFACE_GENERATED_GEODATA));
  scene.remove(SURFACE_GENERATED_GEODATA);
  failures +=
      expect(scene.group_size(second) == 1 && scene.tree_entity_count() == 1,
             "surface removal preserves other entities in a group");

  scene.remove_group(second);
  failures +=
      expect(scene.group_size(second) == 0 && scene.tree_entity_count() == 0,
             "group removal remains valid after surface removal");
  return failures;
}
