#include "pch.h"

#include <unreal/Actor.h>
#include <unreal/Level.h>
#include <unreal/Material.h>
#include <unreal/PackageLoader.h>
#include <unreal/StaticMesh.h>
#include <unreal/Terrain.h>

void load_package(const unreal::PackageLoader &package_loader,
                  const std::string &package_name);

auto main(int argc, char **argv) -> int {
  if (argc < 3) {
    std::cout << "Usage:" << std::endl;
    std::cout << "\tunreal_test <L2 root path> <package name>" << std::endl;
    return EXIT_FAILURE;
  }

  utils::Log::level = utils::LOG_INFO;
  //  utils::Log::level = utils::LOG_ALL;

    const std::filesystem::path root_path{argv[1]};
    const std::string package_name{argv[2]};

    if (!std::filesystem::exists(root_path)) {
      utils::Log(utils::LOG_ERROR)
          << "Directory not found: " << root_path << std::endl;
      return EXIT_FAILURE;
    }

    const std::vector<unreal::SearchConfig> configs{
        unreal::SearchConfig{"MAPS", "unr"},
        unreal::SearchConfig{"StaticMeshes", "usx"},
        unreal::SearchConfig{"Textures", "utx"},
        unreal::SearchConfig{"SysTextures", "utx"}};

    unreal::PackageLoader package_loader{root_path, configs};

    // HF maps
    std::vector<std::string> package_names = {
        "11_23", "11_24", "11_25", "11_26", "12_23", "12_24", "12_25", "12_26",
        "13_21", "13_22", "13_23", "13_24", "13_25", "13_26", "14_21", "14_22",
        "14_23", "14_24", "14_25", "14_26", "15_10", "15_11", "15_12", "15_18",
        "15_19", "15_20", "15_21", "15_22", "15_23", "15_24", "15_25", "15_26",
        "16_10", "16_11", "16_12", "16_13", "16_14", "16_15", "16_16", "16_17",
        "16_18", "16_19", "16_20", "16_21", "16_22", "16_23", "16_24", "16_25",
        "16_26", "17_10", "17_11", "17_12", "17_13", "17_14", "17_15", "17_16",
        "17_17", "17_18", "17_19", "17_20", "17_21", "17_22", "17_23", "17_24",
        "17_25", "17_26", "18_10", "18_11", "18_12", "18_13", "18_14", "18_15",
        "18_16", "18_17", "18_18", "18_19", "18_20", "18_21", "18_22", "18_23",
        "18_24", "18_25", "18_26", "19_10", "19_11", "19_12", "19_13", "19_14",
        "19_15", "19_16", "19_17", "19_18", "19_19", "19_20", "19_21", "19_22",
        "19_23", "19_24", "19_25", "19_26", "20_10", "20_11", "20_12", "20_13",
        "20_14", "20_15", "20_16", "20_17", "20_18", "20_19", "20_20", "20_21",
        "20_22", "20_23", "20_24", "20_25", "20_26", "21_10", "21_11", "21_12",
        "21_13", "21_14", "21_15", "21_16", "21_17", "21_18", "21_19", "21_20",
        "21_21", "21_22", "21_23", "21_24", "21_25", "22_10", "22_11", "22_12",
        "22_13", "22_14", "22_15", "22_16", "22_17", "22_18", "22_19", "22_20",
        "22_21", "22_22", "22_23", "22_24", "22_25", "22_26", "23_10", "23_11",
        "23_12", "23_13", "23_14", "23_15", "23_16", "23_17", "23_18", "23_19",
        "23_20", "23_21", "23_22", "23_23", "23_24", "23_25", "23_26", "24_10",
        "24_11", "24_12", "24_13", "24_14", "24_15", "24_16", "24_17", "24_18",
        "24_19", "24_20", "24_21", "24_22", "24_23", "24_24", "24_25", "24_26",
        "25_10", "25_11", "25_12", "25_14", "25_15", "25_16", "25_17", "25_18",
        "25_19", "25_20", "25_21", "25_22", "25_23", "25_24", "26_11", "26_12",
        "26_14", "26_15", "26_16",
    };

    for (const auto &name : package_names) {
      //      load_package(package_loader, name);
    }

    load_package(package_loader, package_name);

    utils::Log(utils::LOG_INFO) << "Done" << std::endl;

    return EXIT_SUCCESS;
}

void load_package(const unreal::PackageLoader &package_loader,
                  const std::string &package_name) {

  auto optional_package = package_loader.load_package(package_name);

  if (!optional_package.has_value()) {
    return;
  }

  auto &package = optional_package.value();

  utils::Log(utils::LOG_DEBUG) << package << std::endl;

  std::vector<std::shared_ptr<unreal::Model>> models;
  package.load_objects("Model", models);
  utils::Log(utils::LOG_INFO) << "Models: " << models.size() << std::endl;

  std::vector<std::shared_ptr<unreal::Texture>> textures;
  package.load_objects("Texture", textures);
  utils::Log(utils::LOG_INFO) << "Textures: " << textures.size() << std::endl;

  std::vector<std::shared_ptr<unreal::StaticMesh>> meshes;
  package.load_objects("StaticMesh", meshes);
  utils::Log(utils::LOG_INFO) << "Meshes: " << meshes.size() << std::endl;

  std::vector<std::shared_ptr<unreal::StaticMeshActor>> mesh_actors;
  package.load_objects("StaticMeshActor", mesh_actors);
  package.load_objects("MovableStaticMeshActor", mesh_actors);
  package.load_objects("L2MovableStaticMeshActor", mesh_actors);
  utils::Log(utils::LOG_INFO)
      << "Static Mesh Actors: " << mesh_actors.size() << std::endl;

  std::vector<std::shared_ptr<unreal::Shader>> shaders;
  package.load_objects("Shader", shaders);
  utils::Log(utils::LOG_INFO) << "Shaders: " << shaders.size() << std::endl;

  std::vector<std::shared_ptr<unreal::TerrainInfoActor>> terrains;
  package.load_objects("TerrainInfo", terrains);
  utils::Log(utils::LOG_INFO) << "Terrains: " << terrains.size() << std::endl;

  std::vector<std::shared_ptr<unreal::Level>> levels;
  package.load_objects("Level", levels);
  utils::Log(utils::LOG_INFO) << "Levels: " << levels.size() << std::endl;

  std::vector<std::shared_ptr<unreal::BrushActor>> brushes;
  package.load_objects("Brush", brushes);
  utils::Log(utils::LOG_INFO) << "Brushes: " << brushes.size() << std::endl;

  std::vector<std::shared_ptr<unreal::BlockingVolumeActor>> blocking_volumes;
  package.load_objects("BlockingVolume", blocking_volumes);
  utils::Log(utils::LOG_INFO)
      << "Blocking Volumes: " << blocking_volumes.size() << std::endl;

  std::vector<std::shared_ptr<unreal::WaterVolumeActor>> water_volumes;
  package.load_objects("WaterVolume", water_volumes);
  utils::Log(utils::LOG_INFO)
      << "Water Volumes: " << water_volumes.size() << std::endl;

  utils::Log(utils::LOG_INFO)
      << "Level nodes: " << levels[0]->model->nodes.size() << std::endl;
}
