#include "pch.h"

#include "GeodataEntityFactory.h"
#include "LoadingSystem.h"
#include "SceneStats.h"
#include "UnrealLoader.h"

namespace {

void log_geometry_stats(const std::string &map_name, const char *class_name,
                        const GeometryClassStats &stats) {
  utils::Log(utils::LOG_INFO, "App")
      << "Geometry map=" << map_name << " class=" << class_name
      << " actors=" << stats.actors << " vertices=" << stats.vertices
      << " triangles=" << stats.triangles << std::endl;
}

} // namespace

LoadingSystem::LoadingSystem(GeodataContext &geodata_context,
                             const Renderer *renderer,
                             const std::filesystem::path &root_path,
                             const std::vector<std::string> &map_names)
    : m_geodata_context{geodata_context}, m_renderer{renderer} {

  UnrealLoader unreal_loader{root_path};
  geodata::Loader geodata_loader{"geodata"};

  GeodataEntityFactory geodata_entity_factory;

  std::vector<Map> maps;
  std::vector<Entity<GeodataMesh>> geodata_entities;

  for (const auto &map_name : map_names) {
    utils::Log(utils::LOG_INFO, "App")
        << "Loading map: " << map_name << std::endl;

    // Load map entities
    auto map = unreal_loader.load_map(map_name);
    map.name = map_name;

    const auto geometry_stats = map_geometry_stats(map);
    log_geometry_stats(map_name, "terrain", geometry_stats.terrain);
    log_geometry_stats(map_name, "static_mesh", geometry_stats.static_meshes);
    log_geometry_stats(map_name, "csg", geometry_stats.csg);
    log_geometry_stats(map_name, "blocking_volume",
                       geometry_stats.blocking_volumes);

    const auto geometry_complete =
        has_required_preview_geometry(geometry_stats);
    utils::Log(geometry_complete ? utils::LOG_INFO : utils::LOG_WARN, "App")
        << "Geometry verification map=" << map_name
        << " required=terrain,static_mesh,csg,blocking_volume result="
        << (geometry_complete ? "pass" : "incomplete") << std::endl;

    maps.push_back(map);

    if (m_renderer == nullptr) {
      // We are in a CLI mode
      continue;
    }

    // Load geodata
    const auto *geodata = geodata_loader.load_geodata(map_name);

    if (geodata == nullptr) {
      continue;
    }

    auto geodata_entity = geodata_entity_factory.make_entity(
        *geodata, map.bounding_box, SURFACE_IMPORTED_GEODATA);

    geodata_entities.push_back(std::move(geodata_entity));
  }

  if (m_renderer != nullptr) {
    utils::Log(utils::LOG_INFO, "App")
        << "Prepare maps for rendering" << std::endl;
    m_renderer->render_maps(maps);
    m_renderer->render_geodata(geodata_entities);
  }

  utils::Log(utils::LOG_INFO, "App")
      << "Prepare maps for geodata building" << std::endl;
  prebuild_maps(maps);
}

void LoadingSystem::prebuild_maps(const std::vector<Map> &maps) const {
  std::unordered_map<std::shared_ptr<EntityMesh>,
                     std::shared_ptr<geodata::Mesh>>
      entity_mesh_cache;

  for (const auto &map : maps) {
    if (map.entities.empty()) {
      continue;
    }

    geodata::Map geodata_map{map.name, map.bounding_box};

    for (const auto &entity : map.entities) {
      // Load mesh if needed
      auto cached_mesh = entity_mesh_cache.find(entity.mesh);

      if (cached_mesh == entity_mesh_cache.end()) {
        std::vector<geodata::Vertex> vertices;
        std::vector<unsigned int> indices;
        auto skipped_indices = 0;

        for (const auto &surface : entity.mesh->surfaces) {
          if ((surface.type & (SURFACE_PASSABLE | SURFACE_BOUNDING_BOX)) != 0) {
            skipped_indices += surface.index_count;
            continue;
          }

          for (auto i = surface.index_offset;
               i < (surface.index_offset + surface.index_count); ++i) {

            const auto index = entity.mesh->indices[i];

            vertices.push_back({entity.mesh->vertices[index].position,
                                entity.mesh->vertices[index].normal});

            indices.push_back(i - skipped_indices);
          }
        }

        if (vertices.empty() || indices.empty()) {
          entity_mesh_cache.insert({entity.mesh, nullptr});
          continue;
        }

        const auto mesh = std::make_shared<geodata::Mesh>();
        mesh->vertices.swap(vertices);
        mesh->indices.swap(indices);
        mesh->instance_matrices = entity.instance_matrices();

        cached_mesh = entity_mesh_cache.insert({entity.mesh, mesh}).first;
      }

      if (cached_mesh->second == nullptr) {
        continue;
      }

      geodata::Entity geodata_entity{
          cached_mesh->second,
          entity.model_matrix(),
      };

      geodata_map.add(geodata_entity);
    }

    m_geodata_context.maps.push_back(std::move(geodata_map));
  }
}
