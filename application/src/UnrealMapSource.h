#pragma once

#include "MapSource.h"
#include "UnrealLoader.h"
#include <territory/VisualSceneLoader.h>

#include <filesystem>

class UnrealMapSource final : public MapSource {
public:
  explicit UnrealMapSource(const std::filesystem::path &client_root);
  auto load(const MapRegion &region, MapLayer layer,
            const territory::Cancel &cancel) -> MapLoadPayload override;

private:
  UnrealLoader m_loader;
  territory::VisualSceneLoader m_visual_loader;
};
