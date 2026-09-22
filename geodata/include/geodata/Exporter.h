#pragma once

#include "ExportBuffer.h"

#include <filesystem>
#include <string>

namespace geodata {

class Exporter {
public:
  explicit Exporter(const std::filesystem::path &root_path);

  void export_geodata(const ExportBuffer &export_buffer,
                      const std::string &name) const;

private:
  const std::filesystem::path m_root_path;
};

} // namespace geodata
