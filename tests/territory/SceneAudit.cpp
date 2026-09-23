#include <iostream>
#include <territory/PathIO.h>
#include <territory/VisualSceneLoader.h>
#include <unreal/ArchiveLoader.h>
#include <unreal/PropertyExtractor.h>
#include <utils/Log.h>
int scene_audit(const std::filesystem::path &client, const std::string &map,
                const std::filesystem::path &output, bool inventory) {
  using namespace territory;
  utils::Log::level = utils::LOG_ERROR;
  auto dir = create_job_directory(output, client);
  if (!inventory) {
    auto scene = VisualSceneLoader(client).load(map, {});
    validate_scene(scene);
    Json candidates = Json::array(), volumes = Json::array();
    for (const auto &d : scene.draws) {
      const auto &m = scene.library.materials[d.material];
      glm::vec3 lo(std::numeric_limits<float>::max()),
          hi(-std::numeric_limits<float>::max());
      for (std::size_t i = d.first_index; i < d.first_index + d.index_count;
           ++i) {
        auto p =
            glm::vec3(d.transform *
                      glm::vec4(scene.meshes[d.mesh]
                                    .vertices[scene.meshes[d.mesh].indices[i]]
                                    .position,
                                1.f));
        lo = glm::min(lo, p);
        hi = glm::max(hi, p);
      }
      if (hi.z - lo.z > 2 || hi.x - lo.x < 32 || hi.y - lo.y < 32)
        continue;
      if ((m.blend == Blend::Opaque || m.blend == Blend::Masked) && !d.water)
        continue;
      candidates.push_back({{"draw", d.source},
                            {"material", m.source},
                            {"blend", static_cast<int>(m.blend)},
                            {"min", {lo.x, lo.y, lo.z}},
                            {"max", {hi.x, hi.y, hi.z}}});
    }
    unreal::ArchiveLoader loader(client, {unreal::SearchConfig{"Maps", "unr"}});
    auto *a = loader.load_archive(map);
    for (auto &e : a->export_map)
      if (e.class_name == "WaterVolume") {
        auto v = std::dynamic_pointer_cast<unreal::WaterVolumeActor>(
            a->object_loader.export_object(e));
        if (!v || !v->brush.has_reference())
          continue;
        auto b = v->brush.as<unreal::Model>();
        if (!b)
          continue;
        glm::vec3 lo(std::numeric_limits<float>::max()),
            hi(-std::numeric_limits<float>::max());
        for (auto p : b->points) {
          auto w = glm::vec3(visual_actor_transform(*v) *
                             glm::vec4(p.x, p.y, p.z, 1.f));
          lo = glm::min(lo, w);
          hi = glm::max(hi, w);
        }
        volumes.push_back({{"source", v->full_name()},
                           {"points", b->points.size()},
                           {"nodes", b->nodes.size()},
                           {"min", {lo.x, lo.y, lo.z}},
                           {"max", {hi.x, hi.y, hi.z}}});
      }
    scene.report.maps.front()["water_candidate_audit"] = candidates;
    scene.report.maps.front()["water_volume_audit"] = volumes;
    write_json_atomic(dir / "report.json", to_json(scene.report, "scene-audit"),
                      false);
    std::cout << "Scene: meshes=" << scene.meshes.size()
              << " draws=" << scene.draws.size()
              << " terrain=" << scene.terrain_layers.size()
              << " issues=" << scene.report.issues.size() << '\n';
  } else {
    unreal::ArchiveLoader loader(client,
                                 {unreal::SearchConfig{"Maps", "unr"},
                                  unreal::SearchConfig{"Textures", "utx"},
                                  unreal::SearchConfig{"StaticMeshes", "usx"}});
    auto *a = loader.load_archive(map);
    if (!a)
      throw std::runtime_error("Map not found");
    Json classes = Json::object(), props = Json::array();
    for (auto &e : a->export_map) {
      auto type = std::string(e.class_name);
      if (!classes.contains(type))
        classes[type] = 0;
      classes[type] = classes[type].get<int>() + 1;
      if (type != "TerrainInfo" && type != "FluidSurfaceInfo" &&
          type != "WaterVolume" && type != "PhysicsVolume" &&
          type != "Shader" && type != "ColorModifier")
        continue;
      auto &in = static_cast<std::istream &>(*a);
      in.clear();
      in.seekg(e.serial_offset.value);
      if (e.object_flags & unreal::RF_HasStack) {
        unreal::StateFrame s;
        *a >> s;
      }
      auto properties = a->property_extractor.extract_properties();
      Json list = Json::array();
      std::function<Json(const unreal::Property &)> dump =
          [&](const unreal::Property &p) {
            Json j = {{"name", std::string(p.name)},
                      {"type", static_cast<int>(p.type)},
                      {"size", p.size},
                      {"array_index", p.array_index.value}};
            if (p.type == unreal::PropertyType::Float)
              j["value"] = p.float_value;
            if (p.type == unreal::PropertyType::Int)
              j["value"] = p.int32_t_value;
            if (p.type == unreal::PropertyType::Byte)
              j["value"] = p.uint8_t_value;
            if (p.type == unreal::PropertyType::Bool)
              j["value"] = p.bool_value();
            if (p.type == unreal::PropertyType::Object && p.index_value != 0)
              j["value"] = a->object_reference(p.index_value).object_path;
            if (p.struct_name == "Vector")
              j["value"] = {p.vector_value.x, p.vector_value.y,
                            p.vector_value.z};
            if (p.struct_name == "Rotator")
              j["value"] = {p.rotator_value.pitch, p.rotator_value.yaw,
                            p.rotator_value.roll};
            if (!p.subproperties.empty()) {
              j["fields"] = Json::array();
              for (auto &[n, v] : p.subproperties.front())
                j["fields"].push_back(dump(v));
            }
            return j;
          };
      for (auto &p : properties) {
        auto j = dump(p);
        if (!p.subproperties.empty()) {
          j["fields"] = Json::array();
          for (auto &[n, v] : p.subproperties.front())
            j["fields"].push_back(dump(v));
        }
        list.push_back(j);
      }
      Json entry = {{"class", type},
                    {"object", std::string(e.object_name)},
                    {"flags", e.object_flags},
                    {"properties", list}};
      if (type == "Shader") {
        auto shader = std::dynamic_pointer_cast<unreal::Shader>(
            a->object_loader.export_object(e));
        entry["loaded_diffuse"] = shader->diffuse.reference().object_path;
        entry["loaded_opacity"] = shader->opacity.reference().object_path;
      }
      props.push_back(entry);
    }
    write_json_atomic(dir / "report.json",
                      {{"classes", classes}, {"properties", props}}, false);
    std::cout << classes.dump() << '\n';
  }
  std::cout << path_utf8(dir / "report.json") << '\n';
  return 0;
}
