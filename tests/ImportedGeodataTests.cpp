#include "ImportedGeodataLoader.h"
#include "TestSupport.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

class TemporaryGeodataDirectory {
public:
  TemporaryGeodataDirectory() {
    const auto suffix = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    path = std::filesystem::temp_directory_path() /
           ("l2mapconv-imported-geodata-" + suffix);
    std::filesystem::create_directories(path);
  }

  ~TemporaryGeodataDirectory() { std::filesystem::remove_all(path); }

  std::filesystem::path path;
};

void write_simple_l2j(const std::filesystem::path &path, std::int16_t height) {
  std::ofstream output{path, std::ios::binary};
  constexpr auto block_count = 256 * 256;
  for (auto block = 0; block < block_count; ++block) {
    output.put(static_cast<char>(geodata::BLOCK_SIMPLE));
    output.write(reinterpret_cast<const char *>(&height), sizeof(height));
  }
}

} // namespace

auto run_imported_geodata_tests() -> int {
  auto failures = 0;
  const TemporaryGeodataDirectory temporary;
  write_simple_l2j(temporary.path / "22_22.l2j", 128);

  ImportedGeodataLoader loader{temporary.path};
  const geometry::Box bounds{{100.0f, 200.0f, -50.0f},
                             {300.0f, 400.0f, 500.0f}};
  const auto entities = loader.load("22_22", bounds);
  failures += expect(entities.size() == 1,
                     "existing L2J data produces one preview entity");
  if (!entities.empty()) {
    failures += expect(entities.front().mesh->cells.size() == 256U * 256U,
                       "simple L2J blocks are loaded into preview cells");
    failures += expect(entities.front().mesh->surface.type ==
                           SURFACE_IMPORTED_GEODATA,
                       "preview entity uses the imported geodata surface");
    failures += expect(entities.front().position == glm::vec3{100.0f, 200.0f,
                                                               0.0f},
                       "preview entity is placed at the map origin");
  }
  failures += expect(loader.load("missing", bounds).empty(),
                     "missing imported geodata produces no preview entity");
  return failures;
}
