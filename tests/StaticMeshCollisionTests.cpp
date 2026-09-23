#include "TestSupport.h"
#include "UnrealLoader.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <utils/Log.h>

auto run_static_mesh_collision_tests(const std::filesystem::path &client,
                                     const std::string &region) -> int {
  utils::Log::level = utils::LOG_ERROR;
  unreal::PackageLoader parser{client,
                               {unreal::SearchConfig{"Maps", "unr"},
                                unreal::SearchConfig{"StaticMeshes", "usx"},
                                unreal::SearchConfig{"Textures", "utx"},
                                unreal::SearchConfig{"SysTextures", "utx"}}};
  const auto package = parser.load_package(region);
  if (!package)
    return expect(false, "collision fixture map exists");
  std::vector<std::shared_ptr<unreal::StaticMeshActor>> actors;
  for (auto type : {"StaticMeshActor", "MovableStaticMeshActor",
                    "L2MovableStaticMeshActor"}) {
    package->load_objects(type, actors);
  }
  UnrealLoader loader{client};
  const auto map = loader.load_map(region, {false, true, false, false});
  auto failures = 0;
  std::size_t entity_index = 0;
  std::unordered_map<std::string, unsigned int> mesh_policies;
  for (const auto &actor : actors) {
    if (actor->delete_me || actor->hidden || !actor->static_mesh)
      continue;
    if (entity_index >= map.entities.size())
      return expect(false, "actor has a loaded mesh entity");
    const auto &entity = map.entities[entity_index];
    entity_index += 2; // mesh followed by its wireframe bounds
    const auto mesh = actor->static_mesh.as<unreal::StaticMesh>();
    const auto blocks =
        actor->collide_actors && actor->block_actors && actor->block_players;
    mesh_policies[mesh->full_name()] |= blocks ? 2U : 1U;
    std::size_t loaded_surface = 0;
    for (std::size_t i = 0; i < mesh->surfaces.size(); ++i) {
      if (mesh->surfaces[i].triangle_max == 0)
        continue;
      if (i >= mesh->materials.size() ||
          loaded_surface >= entity.mesh->surfaces.size()) {
        return expect(false,
                      "mesh surface has a material and a loaded counterpart");
      }
      const auto &surface = entity.mesh->surfaces[loaded_surface++];
      const auto actual_blocks = (surface.type & SURFACE_PASSABLE) == 0;
      if (actual_blocks != (blocks && mesh->materials[i].enable_collision)) {
        ++failures;
        std::cerr << "FAIL: actor-specific collision " << actor->full_name()
                  << " surface " << i << '\n';
      }
    }
    failures += expect(loaded_surface == entity.mesh->surfaces.size(),
                       "all actor surfaces checked");
  }
  failures += expect(entity_index > 0 && entity_index == map.entities.size(),
                     "collision audit checks every mesh actor");
  const auto mixed_policies =
      std::count_if(mesh_policies.begin(), mesh_policies.end(),
                    [](const auto &entry) { return entry.second == 3; });
  if (region == "25_19") {
    failures += expect(
        mixed_policies > 0,
        "25_19 fixture exercises shared meshes with both collision policies");
  }
  std::cout << region << ": checked " << entity_index / 2
            << " mesh actors, mixed-policy meshes=" << mixed_policies
            << ", failures=" << failures << '\n';
  return failures;
}
