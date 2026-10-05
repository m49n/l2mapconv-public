#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <territory/MaterialResolver.h>
#include <territory/TerrainMapping.h>
#include <territory/TerrainGeometry.h>
#include <territory/VisualSceneLoader.h>
#include <territory/WaterSurface.h>
#include <unreal/ArchiveLoader.h>
#include <unreal/Level.h>
namespace territory {
namespace {
glm::vec3 vec(unreal::Vector v) { return {v.x, v.y, v.z}; }
std::string identity(const unreal::AssetReference &r) {
  return r.package + "." + r.object_path;
}
bool finite(glm::vec3 v) {
  return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
bool finite(const glm::mat4 &m) {
  for (int c = 0; c < 4; ++c)
    for (int r = 0; r < 4; ++r)
      if (!std::isfinite(m[c][r]))
        return false;
  return true;
}
template <class T> const T &at(const std::vector<T> &v, int i) {
  if (i < 0 || static_cast<std::size_t>(i) >= v.size())
    throw std::runtime_error("Visual geometry index out of bounds");
  return v[i];
}
std::string map_name(int x, int y) {
  std::ostringstream s;
  s << std::setfill('0') << std::setw(2) << x << '_' << std::setw(2) << y;
  return s.str();
}
Json matrix_json(const glm::mat4 &m) {
  Json j = Json::array();
  for (int c = 0; c < 4; ++c)
    j.push_back({m[c][0], m[c][1], m[c][2], m[c][3]});
  return j;
}
} // namespace
bool visual_bsp_visible(std::uint32_t f) {
  return !(
      f & (unreal::PF_Invisible | unreal::PF_Portal | unreal::PF_FakeBackdrop));
}
bool include_water(WaterEvidence e, bool on) {
  return on && e == WaterEvidence::Surface;
}
std::string water_surface(const unreal::AssetReference &material,
                          std::span<const glm::vec3> polygon,
                          std::span<const WaterVolumeBounds> volumes) {
  if (material.package != "FX_E_T" ||
      material.object_path != "WaterSurfaceShaderSet.WaterShader01" ||
      material.class_name != "Shader" || polygon.size() < 3)
    return {};
  glm::vec3 center{};
  for (auto p : polygon) {
    if (!finite(p))
      return {};
    center += p;
  }
  center /= static_cast<float>(polygon.size());
  for (auto p : polygon)
    if (std::abs(p.z - center.z) > 2.f)
      return {};
  for (const auto &volume : volumes)
    if (center.x >= volume.min.x && center.x <= volume.max.x &&
        center.y >= volume.min.y && center.y <= volume.max.y &&
        std::abs(center.z - volume.max.z) <= 2.f)
      return volume.source;
  return {};
}
glm::mat4 visual_actor_transform(const unreal::Actor &actor) {
  // UE local point is offset by PrePivot before scale/rotation, then translated
  // into the level.
  return glm::translate(glm::mat4(1.f), vec(actor.location)) *
         glm::mat4_cast(glm::quat(vec(actor.rotation.vector()))) *
         glm::scale(glm::mat4(1.f), vec(actor.scale())) *
         glm::translate(glm::mat4(1.f), -vec(actor.pre_pivot));
}
bool visual_actor_visible(const unreal::Actor &actor, int zone_state) {
  return !actor.hidden && !actor.delete_me &&
         (actor.zone_render_states.empty() ||
          std::any_of(actor.zone_render_states.begin(), actor.zone_render_states.end(),
                      [zone_state](const auto &entry) { return entry.state == zone_state; }));
}
const unreal::MaterialReference &visual_skin(const unreal::Actor &actor,
                                             const unreal::StaticMesh &mesh,
                                             std::size_t i, int zone_state) {
  for (const auto &entry : actor.zone_render_states)
    if (entry.state == zone_state && i < entry.skins.size() && entry.skins[i].has_reference())
      return entry.skins[i];
  if (i < actor.skins.size() && actor.skins[i].has_reference())
    return actor.skins[i];
  return mesh.materials.at(i).material;
}
TerrainMapping terrain_mapping(const unreal::TerrainInfoActor &t,
                               const unreal::TerrainLayer &l) {
  TerrainMapping result;
  result.evidence =
      "Unsupported terrain mapping variant: only regular P542 XY, zero "
      "pan/rotation with serialized layer Scale is evidenced";
  if (t.terrain_scale.x != 128.f || t.terrain_scale.y != 128.f ||
      !finite(vec(t.location)) ||
      l.texture_map_axis != unreal::TEXMAPAXIS_XY || l.u_pan != 0 ||
      l.v_pan != 0 || l.texture_rotation != 0 || l.layer_rotation.pitch ||
      l.layer_rotation.yaw || l.layer_rotation.roll ||
      !std::isfinite(l.u_scale) || !std::isfinite(l.v_scale) ||
      l.u_scale <= 0 || l.v_scale <= 0 || !finite(vec(l.layer_scale)) ||
      l.layer_scale.x <= 0 || l.layer_scale.y <= 0)
    return result;
  // Geometry validation establishes a 32768-unit terrain. UVs are local to
  // its actual origin, not the independent filename-based radar crop.
  const double x0 = double(t.location.x) - 16384.,
               y0 = double(t.location.y) - 16384.;
  const double su = 2. * l.layer_scale.x / (128. * 128. * l.u_scale),
               sv = 2. * l.layer_scale.y / (128. * 128. * l.v_scale);
  result.world_to_uv = glm::mat4(1.f);
  result.world_to_uv[0][0] = static_cast<float>(su);
  result.world_to_uv[1][1] = static_cast<float>(sv);
  result.world_to_uv[3][0] = static_cast<float>(-x0 * su);
  result.world_to_uv[3][1] = static_cast<float>(-y0 * sv);
  result.verified = true;
  result.evidence =
      "Independent viewer control points: justgos/l2mapper "
      "src/L2Lib/UTerrain.cpp UTerrainSector::Init; P542 serialized layer "
      "Scale, XY zero-pan/rotation subset. Not a retail runtime parity claim.";
  return result;
}
void expand_scene_z_bounds(VisualScene &scene) {
  auto include = [&](double z) {
    if (!std::isfinite(z))
      throw std::runtime_error("Non-finite scene height");
    scene.bounds.min_z = std::min(scene.bounds.min_z, z - 128);
    scene.bounds.max_z = std::max(scene.bounds.max_z, z + 128);
  };
  for (const auto &d : scene.draws)
    for (auto i : scene.meshes.at(d.mesh).indices) {
      const auto p =
          d.transform *
          glm::vec4(scene.meshes.at(d.mesh).vertices.at(i).position, 1);
      include(p.z);
    }
  if (scene.terrain_mesh) {
    const auto &mesh = scene.meshes.at(*scene.terrain_mesh);
    for (auto index : mesh.indices)
      include(mesh.vertices.at(index).position.z);
  }
}
void validate_scene(const VisualScene &s) {
  if (!(s.bounds.min_x < s.bounds.max_x && s.bounds.min_y < s.bounds.max_y &&
        s.bounds.min_z <= s.bounds.max_z))
    throw std::runtime_error("Invalid scene bounds");
  for (auto d : {s.bounds.min_x, s.bounds.min_y, s.bounds.max_x, s.bounds.max_y,
                 s.bounds.min_z, s.bounds.max_z})
    if (!std::isfinite(d))
      throw std::runtime_error("Non-finite scene bounds");
  for (const auto &m : s.meshes) {
    for (auto i : m.indices)
      if (i >= m.vertices.size())
        throw std::runtime_error("Visual vertex index out of bounds");
    if (m.indices.size() % 3)
      throw std::runtime_error("Incomplete visual triangle");
    for (const auto &v : m.vertices) {
      if (!finite(v.position) || !finite(v.normal))
        throw std::runtime_error("Non-finite visual vertex");
      for (auto uv : v.uv)
        if (!std::isfinite(uv.x) || !std::isfinite(uv.y))
          throw std::runtime_error("Non-finite visual UV");
    }
  }
  for (const auto &d : s.draws) {
    if (d.mesh >= s.meshes.size() || d.material >= s.library.materials.size() ||
        !finite(d.transform))
      throw std::runtime_error("Invalid visual draw reference");
    const auto count = s.meshes[d.mesh].indices.size();
    if (d.first_index > count || d.index_count > count - d.first_index ||
        d.index_count % 3)
      throw std::runtime_error("Invalid visual draw range");
  }
  if (s.terrain_mesh && *s.terrain_mesh >= s.meshes.size())
    throw std::runtime_error("Invalid terrain mesh");
  for (const auto &l : s.terrain_layers)
    if (l.material >= s.library.materials.size() ||
        l.mask_texture >= s.library.textures.size() || !finite(l.world_to_uv))
      throw std::runtime_error("Invalid terrain layer reference");
  for (const auto &m : s.library.materials) {
    material_expression(m);
    for (const auto &n : m.nodes)
      if (n.texture && *n.texture >= s.library.textures.size())
        throw std::runtime_error("Invalid texture reference");
  }
}
VisualScene VisualSceneLoader::load(const std::string &name,
                                    const Cancel &cancel) {
  if (name.size() != 5 || name[2] != '_' ||
      !std::isdigit(static_cast<unsigned char>(name[0])) ||
      !std::isdigit(static_cast<unsigned char>(name[1])) ||
      !std::isdigit(static_cast<unsigned char>(name[3])) ||
      !std::isdigit(static_cast<unsigned char>(name[4])))
    throw std::runtime_error("Invalid map name");
  check_cancel(cancel);
  unreal::ArchiveLoader loader(client_,
                               {unreal::SearchConfig{"Maps", "unr"},
                                unreal::SearchConfig{"Textures", "utx"},
                                unreal::SearchConfig{"SysTextures", "utx"},
                                unreal::SearchConfig{"StaticMeshes", "usx"}});
  auto *archive = loader.load_archive(name);
  if (!archive)
    throw std::runtime_error("Map package not found: " + name);
  VisualScene scene;
  const int mx = std::stoi(name.substr(0, 2)),
            my = std::stoi(name.substr(3, 2));
  scene.bounds = {(mx - 20) * 32768.,
                  (my - 18) * 32768.,
                  (mx - 19) * 32768.,
                  (my - 17) * 32768.,
                  -16384,
                  16384};
  auto issue = [&](IssueKind k, const std::string &src,
                   const std::string &why) {
    scene.report.issues.push_back({k, src, why, {src}});
  };
  MaterialResolver resolver(scene.library, scene.report);
  resolver.set_package_probe([&](std::string_view p) {
    for (auto f : {"Textures", "SysTextures", "StaticMeshes", "Maps"})
      for (auto ext : {".utx", ".usx", ".unr"})
        if (std::filesystem::is_regular_file(client_ / f /
                                             (std::string(p) + ext)))
          return true;
    return false;
  });
  auto material = [&](const auto &ref, const std::string &source) {
    try {
      return resolver.resolve_object(
          ref.has_reference() ? ref.untyped() : nullptr,
          ref.has_reference() ? ref.reference() : unreal::AssetReference{},
          source);
    } catch (const std::exception &e) {
      issue(IssueKind::Corrupt, source, e.what());
      return resolver.resolve(nullptr, {}, source);
    }
  };
  auto neutral = [&](const std::string &source) {
    return resolver.resolve(nullptr, {}, source);
  };
  auto uv_check = [&](std::size_t id, const std::array<bool, 4> &channels,
                      const std::string &label) {
    auto copy = scene.library.materials[id];
    const auto missing = neutralize_missing_uv_samples(copy, channels);
    if (missing.empty())
      return id;
    copy.source += " [unsupported UV channel]";
    for (auto channel : missing)
      issue(IssueKind::Unsupported, label,
            "Material " + copy.source + " requires absent UV channel " +
                std::to_string(channel) +
                "; only this sample is neutralized");
    scene.library.materials.push_back(std::move(copy));
    return scene.library.materials.size() - 1;
  };
  Json terrain_info = Json::array(), terrain_geometry = nullptr, classes = Json::object(),
       water = Json::array();
  std::size_t zone_filtered_actors = 0;
  std::size_t static_actors = 0, bsp_faces = 0, water_volumes = 0,
              water_surfaces = 0;
  std::vector<WaterVolumeBounds> volume_bounds;
  std::map<std::string, std::pair<std::size_t, std::array<bool, 4>>> mesh_cache;
  std::map<std::string, std::vector<std::string>> unsupported;
  for (auto &e : archive->export_map) {
    auto c = std::string(e.class_name);
    classes[c] = classes.value(c, 0) + 1;
  }
  for (auto &e : archive->export_map)
    if (e.class_name == "WaterVolume") {
      check_cancel(cancel);
      ++water_volumes;
      const auto label = name + "." + std::string(e.object_name);
      try {
        auto volume = std::dynamic_pointer_cast<unreal::WaterVolumeActor>(
            archive->object_loader.export_object(e));
        if (!volume || !volume->brush.has_reference())
          continue;
        auto model = volume->brush.as<unreal::Model>();
        if (!model || model->points.empty())
          continue;
        auto transform = visual_actor_transform(*volume);
        WaterVolumeBounds bounds{glm::vec3(std::numeric_limits<float>::max()),
                                 glm::vec3(-std::numeric_limits<float>::max()),
                                 label};
        for (auto p : model->points) {
          auto v = glm::vec3(transform * glm::vec4(vec(p), 1.f));
          if (!finite(v))
            throw std::runtime_error("Invalid water volume bound");
          bounds.min = glm::min(bounds.min, v);
          bounds.max = glm::max(bounds.max, v);
        }
        volume_bounds.push_back(bounds);
        water.push_back({{"source", label},
                         {"evidence", "VolumeOnly"},
                         {"surface_z", bounds.max.z}});
      } catch (const std::exception &ex) {
        issue(IssueKind::Corrupt, label, ex.what());
      }
    }
  auto load_terrain =
      [&](unreal::Archive *a) -> std::shared_ptr<unreal::TerrainInfoActor> {
    if (!a)
      return {};
    for (auto &e : a->export_map)
      if (e.class_name == "TerrainInfo")
        return std::dynamic_pointer_cast<unreal::TerrainInfoActor>(
            a->object_loader.export_object(e));
    return {};
  };
  auto terrain = load_terrain(archive);
  if (terrain) {
    check_cancel(cancel);
    const auto src = identity(terrain->asset_reference());
    std::array<std::shared_ptr<unreal::TerrainInfoActor>, 4> edges{
        terrain, nullptr, nullptr, nullptr};
    for (int n = 1; n < 4; ++n) {
      check_cancel(cancel);
      const auto neighbour = map_name(mx + (n & 1), my + ((n >> 1) & 1));
      try {
        edges[n] = load_terrain(loader.load_archive(neighbour));
      } catch (const std::exception &e) {
        issue(IssueKind::Corrupt, neighbour, e.what());
      }
      if (edges[n] && (edges[n]->broken_scale() ||
                       !edges[n]->terrain_map.has_reference()))
        edges[n].reset();
      if (!edges[n])
        issue(IssueKind::Simplified, neighbour,
              "Terrain border neighbour unavailable; use other heightmap "
              "coverage where available, otherwise clamp owner height");
    }
    auto geometry = build_terrain_mesh(edges, scene.bounds, cancel);
    if (geometry.clamped_border_samples)
      issue(IssueKind::Simplified, src,
            "Terrain border has " +
                std::to_string(geometry.clamped_border_samples) +
                " samples outside available heightmap coverage; preferred "
                "neighbour/owner height clamped, geometry not moved");
    const auto origin = vec(terrain->position());
    terrain_geometry = {
        {"source", src},
        {"origin", {origin.x, origin.y, origin.z}},
        {"offset_from_square", {origin.x - scene.bounds.min_x,
                                 origin.y - scene.bounds.min_y}},
        {"crop", "filename square; terrain is not snapped"},
        {"border", "world-coordinate triangle sampling; uncovered samples "
                   "clamp preferred neighbour or owner height"},
        {"clamped_border_samples", geometry.clamped_border_samples}};
    scene.terrain_mesh = scene.meshes.size();
    scene.meshes.push_back(std::move(geometry.mesh));
    for (std::size_t slot = 0; slot < terrain->layers.size(); ++slot) {
      check_cancel(cancel);
      const auto &l = terrain->layers[slot];
      const auto label = src + ":layer:" + std::to_string(slot);
      if (!l.texture.has_reference() && !l.alpha_map.has_reference())
        continue;
      if (!l.texture.has_reference() || !l.alpha_map.has_reference()) {
        issue(IssueKind::MissingObject, label,
              "Terrain slot has only one of texture and alpha map");
        continue;
      }
      const auto mapping = terrain_mapping(*terrain, l);
      terrain_info.push_back(
          {{"slot", slot},
           {"texture", identity(l.texture.reference())},
           {"mask", identity(l.alpha_map.reference())},
           {"mapping_verified", mapping.verified},
           {"evidence", mapping.evidence},
           {"world_to_uv_columns", matrix_json(mapping.world_to_uv)}});
      if (!mapping.verified) {
        issue(IssueKind::Unsupported, label, mapping.evidence);
        continue;
      }
      try {
        auto mask = l.alpha_map.as<unreal::Texture>();
        if (!mask) {
          issue(IssueKind::MissingObject, identity(l.alpha_map.reference()),
                "Terrain alpha map did not resolve");
          continue;
        }
        const auto maskid = scene.library.textures.size();
        auto data = texture_data(*mask, TextureUsage::Mask);
        data.clamp_u = data.clamp_v = true;
        scene.library.textures.push_back(std::move(data));
        auto mid = material(l.texture, label);
        mid = static_cast<std::uint32_t>(
            uv_check(mid, {true, false, false, false}, label));
        scene.terrain_layers.push_back({mid, maskid, mapping.world_to_uv});
      } catch (const std::exception &e) {
        issue(IssueKind::Corrupt, label, e.what());
      }
    }
    if (scene.terrain_layers.empty())
      issue(IssueKind::Unsupported, src,
            "No supported terrain color layers; neutral surface will be "
            "rendered");
  } else
    issue(IssueKind::MissingObject, name, "No TerrainInfo export");
  for (auto &e : archive->export_map) {
    check_cancel(cancel);
    const auto type = std::string(e.class_name),
               label = name + "." + std::string(e.object_name);
    if (type == "WaterVolume")
      continue;
    if (type == "StaticMeshActor" || type == "MovableStaticMeshActor" ||
        type == "L2MovableStaticMeshActor") {
      try {
        auto actor = std::dynamic_pointer_cast<unreal::Actor>(
            archive->object_loader.export_object(e));
        if (!actor || actor->hidden || actor->delete_me ||
            !actor->static_mesh.has_reference())
          continue;
        if (!visual_actor_visible(*actor)) {
          ++zone_filtered_actors;
          continue;
        }
        auto mesh = actor->static_mesh.as<unreal::StaticMesh>();
        if (!mesh) {
          issue(IssueKind::MissingObject, label, "Static mesh did not resolve");
          continue;
        }
        ++static_actors;
        const auto key = identity(mesh->asset_reference());
        auto found = mesh_cache.find(key);
        if (found == mesh_cache.end()) {
          VisualMesh out;
          std::array<bool, 4> channels{};
          out.vertices.resize(mesh->vertex_stream.vertices.size());
          for (std::size_t i = 0; i < out.vertices.size(); ++i) {
            out.vertices[i].position =
                vec(mesh->vertex_stream.vertices[i].location);
            out.vertices[i].normal =
                vec(mesh->vertex_stream.vertices[i].normal);
            if (i < mesh->color_stream.colors.size()) {
              auto c = mesh->color_stream.colors[i];
              out.vertices[i].color = {c.r / 255.f, c.g / 255.f, c.b / 255.f,
                                       c.a / 255.f};
            }
          }
          for (const auto &stream : mesh->uv_stream) {
            if (stream.coordinate_index >= 4)
              continue;
            if (stream.uvs.size() != out.vertices.size()) {
              issue(IssueKind::Corrupt, key, "UV stream vertex count mismatch");
              continue;
            }
            channels[stream.coordinate_index] = true;
            for (std::size_t i = 0; i < out.vertices.size(); ++i)
              out.vertices[i].uv[stream.coordinate_index] = {stream.uvs[i].u,
                                                             stream.uvs[i].v};
          }
          out.indices.assign(mesh->index_stream.indices.begin(),
                             mesh->index_stream.indices.end());
          for (auto index : out.indices)
            if (index >= out.vertices.size())
              throw std::runtime_error("Mesh vertex index out of bounds");
          if (out.indices.size() % 3)
            throw std::runtime_error("Mesh index count not divisible by three");
          for (std::size_t i = 0; i < out.indices.size(); i += 3)
            std::swap(out.indices[i], out.indices[i + 2]);
          const auto id = scene.meshes.size();
          scene.meshes.push_back(std::move(out));
          found = mesh_cache.emplace(key, std::make_pair(id, channels)).first;
        }
        const auto transform = visual_actor_transform(*actor);
        for (std::size_t slot = 0; slot < mesh->surfaces.size(); ++slot) {
          auto surf = mesh->surfaces[slot];
          auto start = std::size_t(surf.first_index),
               count = std::size_t(surf.triangle_max) * 3;
          const auto &indices = scene.meshes[found->second.first].indices;
          if (start > indices.size() || count > indices.size() - start)
            throw std::runtime_error("Mesh surface range out of bounds");
          auto src = label + ":" + std::to_string(slot);
          std::size_t id = slot < mesh->materials.size()
                               ? material(visual_skin(*actor, *mesh, slot), src)
                               : neutral(src);
          id = uv_check(id, found->second.second, src);
          scene.draws.push_back(
              {found->second.first, id, start, count, transform, false, src});
          scene.draws.back().passable =
              !(actor->collide_actors && actor->block_actors && actor->block_players &&
                slot < mesh->materials.size() && mesh->materials[slot].enable_collision);
        }
      } catch (const std::exception &ex) {
        issue(IssueKind::Corrupt, label, ex.what());
      }
    } else if (type == "Level") {
      auto level = std::dynamic_pointer_cast<unreal::Level>(
          archive->object_loader.export_object(e));
      if (!level || !level->model.has_reference())
        continue;
      auto model = level->model.as<unreal::Model>();
      if (!model)
        continue;
      for (std::size_t ni = 0; ni < model->nodes.size(); ++ni) {
        check_cancel(cancel);
        const auto &node = model->nodes[ni];
        if (node.vertex_count < 3)
          continue;
        auto src = name + ".BSP:" + std::to_string(ni);
        try {
          const auto &surface = at(model->surfaces, node.surface_index);
          if (!visual_bsp_visible(surface.polygon_flags))
            continue;
          const auto base = vec(at(model->points, surface.base_index)),
                     normal = vec(at(model->vectors, surface.normal_index));
          const auto u = vec(at(model->vectors, surface.u_index)),
                     v = vec(at(model->vectors, surface.v_index));
          auto mid = material(surface.material, src);
          auto copy = scene.library.materials[mid];
          if (surface.polygon_flags & unreal::PF_Masked)
            copy.alpha_test = true;
          if (surface.polygon_flags & unreal::PF_TwoSided)
            copy.two_sided = true;
          copy.unlit = (surface.polygon_flags & unreal::PF_Unlit) != 0;
          if (surface.polygon_flags & unreal::PF_Translucent) {
            copy.blend = Blend::Translucent;
            copy.depth_write = false;
          }
          if (surface.polygon_flags & unreal::PF_Modulated) {
            copy.blend = Blend::Modulate;
            copy.depth_write = false;
          }
          glm::vec2 size{64, 64};
          bool bitmap = false;
          for (auto &n : copy.nodes)
            if (n.texture) {
              auto &tex = scene.library.textures[*n.texture];
              size = {tex.width, tex.height};
              bitmap = true;
              break;
            }
          if (!bitmap && surface.material.has_reference())
            issue(
                IssueKind::Simplified, src,
                "BSP bitmap dimensions unavailable; neutral UV basis 64 used");
          if (surface.polygon_flags &
              (unreal::PF_AutoUPan | unreal::PF_AutoVPan |
               unreal::PF_SmallWavy | unreal::PF_BigWavy))
            issue(IssueKind::Simplified, src,
                  "BSP surface animation frozen without wave deformation");
          scene.library.materials.push_back(std::move(copy));
          mid = static_cast<std::uint32_t>(
              uv_check(scene.library.materials.size() - 1,
                       {true, false, false, false}, src));
          VisualMesh out;
          for (int j = 0; j < node.vertex_count; ++j) {
            auto point =
                vec(at(model->points,
                       at(model->vertices, int(node.vertex_pool_index) + j)
                           .vertex_index));
            VisualVertex vertex;
            vertex.position = point;
            vertex.normal = normal;
            vertex.uv[0] = {glm::dot(point - base, u) / size.x,
                            glm::dot(point - base, v) / size.y};
            out.vertices.push_back(vertex);
          }
          for (std::uint32_t j = 2; j < node.vertex_count; ++j)
            out.indices.insert(out.indices.end(), {0, j - 1, j});
          std::vector<glm::vec3> polygon;
          for (auto &vertex : out.vertices)
            polygon.push_back(vertex.position);
          const auto water_volume = water_surface(surface.material.reference(),
                                                  polygon, volume_bounds);
          if (!water_volume.empty()) {
            ++water_surfaces;
            water.push_back(
                {{"source", src},
                 {"evidence", "Surface"},
                 {"material", identity(surface.material.reference())},
                 {"physical_volume", water_volume}});
          }
          auto count = out.indices.size();
          auto mesh = scene.meshes.size();
          scene.meshes.push_back(std::move(out));
          scene.draws.push_back({mesh, mid, 0, count, glm::mat4(1.f),
                                 !water_volume.empty(), src});
          scene.draws.back().passable =
              (node.flags & unreal::NF_Passable) != 0 ||
              (surface.polygon_flags & unreal::PF_Passable) != 0;
          ++bsp_faces;
        } catch (const std::exception &ex) {
          issue(IssueKind::Corrupt, src, ex.what());
        }
      }
    } else {
      static const std::set<std::string> visual_types = {
          "Mover",       "FluidSurfaceInfo", "FluidSurfaceInfo2",
          "Emitter",     "MeshEmitter",      "SpriteEmitter",
          "BeamEmitter", "VertMeshEmitter",  "SkeletalMeshActor",
          "DecoLayer",   "NMovableSunLight", "NSun",
          "NMoon",       "L2FogInfo"};
      if (visual_types.contains(type))
        unsupported[type].push_back(label);
    }
  }
  for (auto &[type, objects] : unsupported)
    scene.report.issues.push_back(
        {IssueKind::Unsupported, type,
         "Visual actor class not implemented (no placeholder geometry)",
         objects});
  if (water_volumes && !water_surfaces)
    issue(IssueKind::WaterUnresolved, name,
          "No evidenced visual water surface matched the physical WaterVolume "
          "exports; no synthetic volume faces were added");
  if (water_surfaces)
    issue(IssueKind::Simplified, name,
          "Water uses actual BSP surface geometry and material at fixed time; "
          "dynamic reflections/refraction are not reproduced");
  expand_scene_z_bounds(scene);
  scene.report.maps.push_back({{"map", name},
                               {"bounds",
                                {{"min_x", scene.bounds.min_x},
                                 {"min_y", scene.bounds.min_y},
                                 {"max_x", scene.bounds.max_x},
                                 {"max_y", scene.bounds.max_y},
                                 {"min_z", scene.bounds.min_z},
                                 {"max_z", scene.bounds.max_z}}},
                               {"static_mesh_actors", static_actors},
                               {"zone_state", normal_zone_state},
                               {"zone_filtered_actors", zone_filtered_actors},
                               {"bsp_faces", bsp_faces},
                               {"meshes", scene.meshes.size()},
                               {"draws", scene.draws.size()},
                               {"materials", scene.library.materials.size()},
                               {"textures", scene.library.textures.size()},
                               {"terrain_layers", terrain_info},
                               {"terrain_geometry", terrain_geometry},
                               {"water", water},
                               {"water_surfaces", water_surfaces},
                               {"actor_inventory", classes},
                               {"time_seconds", 0}});
  validate_scene(scene);
  return scene;
}
} // namespace territory
