#pragma once

#include "Entity.h"

#include <geodata/Loader.h>

#include <filesystem>
#include <string>
#include <vector>

class ImportedGeodataLoader {
public:
  explicit ImportedGeodataLoader(const std::filesystem::path &root_path);

  auto load(const std::string &map_name,
            const geometry::Box &map_bounding_box) const
      -> std::vector<Entity<GeodataMesh>>;

private:
  geodata::Loader m_loader;
};
