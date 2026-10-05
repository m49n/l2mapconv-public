#pragma once

#include "GeodataContext.h"
#include "Map.h"
#include "Renderer.h"
#include "System.h"
#include <unreal/ArchiveLoader.h>

#include <filesystem>
#include <string>
#include <vector>

class LoadingSystem : public System {
public:
  explicit LoadingSystem(GeodataContext &geodata_context,
                         const Renderer *renderer,
                         const std::filesystem::path &root_path,
                         const std::vector<std::string> &map_names,
                         unreal::ArchiveReadObserver observer = {});

private:
  GeodataContext &m_geodata_context;
  const Renderer *m_renderer;

  void prebuild_maps(const std::vector<Map> &maps) const;
};
