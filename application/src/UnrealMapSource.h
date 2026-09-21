#pragma once

#include "MapSource.h"
#include "UnrealLoader.h"

#include <filesystem>

class UnrealMapSource final : public MapSource {
public:
  explicit UnrealMapSource(const std::filesystem::path &client_root);
  auto load(const MapRegion &region, MapLayer layer) -> Map override;

private:
  UnrealLoader m_loader;
};
