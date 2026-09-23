#include <iostream>
#include <set>
#include <territory/MaterialResolver.h>
#include <territory/PathIO.h>
#include <unreal/Actor.h>
#include <unreal/ArchiveLoader.h>
#include <utils/Log.h>
int material_audit(const std::filesystem::path &client, const std::string &map,
                   const std::filesystem::path &output) {
  using namespace territory;
  utils::Log::level = utils::LOG_ERROR;
  auto directory = create_job_directory(output, client);
  unreal::ArchiveLoader loader(client,
                               {unreal::SearchConfig{"Maps", "unr"},
                                unreal::SearchConfig{"Textures", "utx"},
                                unreal::SearchConfig{"SysTextures", "utx"},
                                unreal::SearchConfig{"StaticMeshes", "usx"}});
  auto *archive = loader.load_archive(map);
  if (!archive)
    throw std::runtime_error("audit map does not exist");
  MaterialLibrary library;
  Report report;
  MaterialResolver resolver(library, report);
  resolver.set_package_probe([&](std::string_view package) {
    for (auto folder : {"Maps", "Textures", "SysTextures", "StaticMeshes"})
      for (auto ext : {".unr", ".utx", ".usx"})
        if (std::filesystem::is_regular_file(client / folder /
                                             (std::string(package) + ext)))
          return true;
    return false;
  });
  std::size_t slots = 0, actors = 0;
  for (auto type : {"StaticMeshActor", "MovableStaticMeshActor",
                    "L2MovableStaticMeshActor"}) {
    std::vector<std::shared_ptr<unreal::StaticMeshActor>> list;
    archive->load_objects(type, list);
    for (auto &actor : list) {
      ++actors;
      try {
        auto mesh = actor->static_mesh.as<unreal::StaticMesh>();
        if (!mesh)
          continue;
        for (std::size_t i = 0; i < mesh->materials.size(); ++i) {
          ++slots;
          const auto &ref = mesh->materials[i].material;
          const auto label = actor->full_name() + ":" + std::to_string(i);
          try {
            resolver.resolve_object(ref.untyped(), ref.reference(), label);
          } catch (const std::exception &e) {
            report.issues.push_back({IssueKind::Corrupt,
                                     ref.reference().object_path,
                                     e.what(),
                                     {label}});
          }
        }
      } catch (const std::exception &e) {
        report.issues.push_back({IssueKind::Corrupt,
                                 actor->full_name(),
                                 e.what(),
                                 {actor->full_name()}});
      }
    }
  }
  const std::set<std::string> classes = {
      "Texture",       "Shader",    "FinalBlend",     "Combiner",
      "TexModifier",   "TexScaler", "TexPanner",      "TexRotator",
      "TexOscillator", "TexEnvMap", "TexCoordSource", "Cubemap",
      "ConstantColor"};
  for (std::size_t i = 0; i < archive->import_map.size(); ++i) {
    const auto &import = archive->import_map[i];
    if (!classes.contains(std::string(import.class_name)))
      continue;
    auto index = unreal::Index{-static_cast<int>(i) - 1};
    const auto ref = archive->object_reference(index);
    try {
      resolver.resolve_object(archive->object_loader.load_object(index), ref,
                              "map-import:" + std::to_string(i));
    } catch (const std::exception &e) {
      report.issues.push_back({IssueKind::Corrupt,
                               ref.object_path,
                               e.what(),
                               {"map-import:" + std::to_string(i)}});
    }
  }
  Json materials = Json::array(), textures = Json::array();
  for (const auto &material : library.materials) {
    Json nodes = Json::array();
    for (const auto &node : material.nodes) {
      Json n = {
          {"op", static_cast<int>(node.op)},
          {"inputs", node.inputs},
          {"uv_channel", node.uv_channel},
          {"value", {node.value.r, node.value.g, node.value.b, node.value.a}}};
      if (node.texture)
        n["texture"] = *node.texture;
      nodes.push_back(n);
    }
    materials.push_back({{"source", material.source},
                         {"root", material.root},
                         {"nodes", nodes},
                         {"blend", static_cast<int>(material.blend)},
                         {"two_sided", material.two_sided},
                         {"depth_write", material.depth_write},
                         {"alpha_ref", material.alpha_ref},
                         {"alpha_test", material.alpha_test}});
  }
  for (const auto &t : library.textures)
    textures.push_back({{"source", t.source},
                        {"width", t.width},
                        {"height", t.height},
                        {"usage", static_cast<int>(t.usage)},
                        {"encoding", static_cast<int>(t.encoding)},
                        {"bytes", t.bytes.size()}});
  report.maps.push_back({{"map", map},
                         {"actors", actors},
                         {"material_slots", slots},
                         {"materials", materials},
                         {"textures", textures}});
  write_json_atomic(directory / "report.json",
                    to_json(report, "material-audit"), false);
  std::cout << "Material audit: actors=" << actors << " slots=" << slots
            << " materials=" << library.materials.size()
            << " textures=" << library.textures.size()
            << " issues=" << report.issues.size() << "\n"
            << path_utf8(directory / "report.json") << '\n';
  return 0;
}
