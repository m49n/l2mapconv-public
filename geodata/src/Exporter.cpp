#include "pch.h"

#include <geodata/Exporter.h>

#include "L2JSerializer.h"
#include "PtsSerializer.h"

#include <cctype>
#include <charconv>
#include <chrono>
#include <stdexcept>
#include <string_view>

namespace {

auto region_coordinate(std::string_view name, std::size_t start,
                       std::size_t end) -> std::uint8_t {
  unsigned int coordinate = 0;
  const auto field = name.substr(start, end - start);
  if (field.empty()) {
    throw std::invalid_argument{"PTS geodata needs a numeric region name"};
  }
  const auto [cursor, error] =
      std::from_chars(field.data(), field.data() + field.size(), coordinate);
  if (error != std::errc{} || cursor != field.data() + field.size() ||
      coordinate > 255) {
    throw std::invalid_argument{"PTS geodata needs a numeric region name"};
  }
  return static_cast<std::uint8_t>(coordinate);
}

class StagingDirectory {
public:
  explicit StagingDirectory(const std::filesystem::path &root) {
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    for (auto attempt = 0; attempt < 32; ++attempt) {
      const auto candidate =
          root / (".l2mapconv-export-" + std::to_string(stamp) + "-" +
                  std::to_string(attempt));
      std::error_code error;
      if (std::filesystem::create_directory(candidate, error)) {
        path = candidate;
        return;
      }
      if (error && error != std::errc::file_exists) {
        throw std::filesystem::filesystem_error{
            "Failed to create geodata staging directory", candidate, error};
      }
    }
    throw std::runtime_error{"Failed to reserve geodata staging directory"};
  }

  ~StagingDirectory() {
    std::error_code ignored;
    std::filesystem::remove_all(path, ignored);
  }

  StagingDirectory(const StagingDirectory &) = delete;
  auto operator=(const StagingDirectory &) -> StagingDirectory & = delete;

  std::filesystem::path path;
};

template <typename Writer>
void write_staged_file(const std::filesystem::path &path, Writer writer) {
  std::ofstream output{path, std::ios::binary};
  if (!output) {
    throw std::runtime_error{"Failed to open staged geodata file"};
  }
  writer(output);
  output.close();
  if (!output) {
    throw std::runtime_error{"Failed to write staged geodata file"};
  }
}

} // namespace

namespace geodata {

Exporter::Exporter(const std::filesystem::path &root_path)
    : m_root_path{root_path} {
  std::filesystem::create_directories(m_root_path);
}

void Exporter::export_geodata(const ExportBuffer &buffer,
                              const std::string &name, bool client_dat) const {
  if (name.empty() ||
      !std::all_of(name.begin(), name.end(), [](unsigned char character) {
        return std::isalnum(character) || character == '_' || character == '-';
      })) {
    throw std::invalid_argument{"Invalid geodata region name"};
  }
  const auto first_separator = name.find('_');
  if (first_separator == std::string::npos) {
    throw std::invalid_argument{"PTS geodata needs an XX_YY region name"};
  }
  const auto second_separator = name.find('_', first_separator + 1);
  const auto region_x = region_coordinate(name, 0, first_separator);
  const auto region_y = region_coordinate(
      name, first_separator + 1,
      second_separator == std::string::npos ? name.size() : second_separator);

  const auto l2j_path = m_root_path / (name + ".l2j");
  const auto pts_path = m_root_path / (name + "_conv.dat");
  if (std::filesystem::exists(l2j_path) ||
      (client_dat && std::filesystem::exists(pts_path))) {
    throw std::runtime_error{"Geodata output already exists"};
  }

  const StagingDirectory staging{m_root_path};
  const auto staged_l2j = staging.path / l2j_path.filename();
  const auto staged_pts = staging.path / pts_path.filename();

  write_staged_file(staged_l2j, [&](std::ostream &output) {
    L2JSerializer serializer;
    serializer.serialize(buffer, output);
  });
  if (client_dat) {
    write_staged_file(staged_pts, [&](std::ostream &output) {
      PtsSerializer serializer;
      serializer.serialize(buffer, region_x, region_y, output);
    });
  }

  std::filesystem::create_hard_link(staged_l2j, l2j_path);
  try {
    if (client_dat)
      std::filesystem::create_hard_link(staged_pts, pts_path);
  } catch (...) {
    std::error_code ignored;
    std::filesystem::remove(l2j_path, ignored);
    throw;
  }

  utils::Log(utils::LOG_INFO, "Geodata")
      << "Geodata exported: " << l2j_path
      << (client_dat ? " (client DAT included)" : "") << std::endl;
}

} // namespace geodata
