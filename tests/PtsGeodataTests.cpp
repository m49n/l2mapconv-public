#include "TestSupport.h"

#include <geodata/Exporter.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

class TemporaryGeodataDirectory {
public:
  TemporaryGeodataDirectory() {
    const auto suffix = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    path = std::filesystem::temp_directory_path() /
           ("l2mapconv-pts-geodata-" + suffix);
    std::filesystem::create_directories(path);
  }

  ~TemporaryGeodataDirectory() { std::filesystem::remove_all(path); }

  std::filesystem::path path;
};

auto matches(const std::vector<std::uint8_t> &bytes, std::size_t offset,
             std::initializer_list<std::uint8_t> expected) -> bool {
  return offset + expected.size() <= bytes.size() &&
         std::equal(expected.begin(), expected.end(), bytes.begin() + offset);
}

auto read_bytes(const std::filesystem::path &path)
    -> std::vector<std::uint8_t> {
  std::ifstream input{path, std::ios::binary};
  return {std::istreambuf_iterator<char>{input},
          std::istreambuf_iterator<char>{}};
}

} // namespace

auto run_pts_geodata_tests() -> int {
  auto failures = 0;

  // A wrong header, block marker, byte order, or layer count must fail this
  // fixture; expected bytes are hand-derived from the PTS layout.
  geodata::Geodata source;
  for (auto cx = 0; cx < 8; ++cx) {
    for (auto cy = 0; cy < 8; ++cy) {
      const auto first = cx == 0 && cy == 0;
      source.cells.push_back(
          {static_cast<std::int16_t>(cx), static_cast<std::int16_t>(cy),
           static_cast<std::int16_t>(first ? 24 : 16), geodata::BLOCK_COMPLEX,
           true, !first, !first, true});
      source.cells.push_back(
          {static_cast<std::int16_t>(cx), static_cast<std::int16_t>(cy + 8), 8,
           geodata::BLOCK_MULTILAYER, true, true, true, true});
    }
  }
  source.cells.push_back(
      {0, 8, 24, geodata::BLOCK_MULTILAYER, true, false, false, false});

  geodata::ExportBuffer buffer;
  buffer.reset(source);
  buffer.set_block_height(1, 0, -80);

  const TemporaryGeodataDirectory temporary;
  const auto output_directory = temporary.path / "output";
  geodata::Exporter exporter{output_directory};
  exporter.export_geodata(buffer, "24_18");

  const auto path = output_directory / "24_18_conv.dat";
  const auto l2j_path = output_directory / "24_18.l2j";
  failures += expect(std::filesystem::is_regular_file(l2j_path),
                     "one export publishes an L2J file alongside PTS");
  failures += expect(std::filesystem::is_regular_file(path),
                     "PTS export uses the client geodata file name");
  if (!std::filesystem::is_regular_file(path)) {
    return failures;
  }

  const auto bytes = read_bytes(path);
  failures += expect(bytes.size() == 393612,
                     "PTS blocks use the expected encoded sizes");
  // 64 complex cells plus 65 serialized multilayer cells.
  failures += expect(matches(bytes, 0,
                             {24, 18, 0x80, 0, 0x10, 0, 0x81, 0, 0, 0, 0xff,
                              0xff, 0, 0, 0xfe, 0xff, 0, 0}),
                     "PTS header counts every serialized multilayer cell");
  failures += expect(matches(bytes, 18, {0x40, 0, 0x39, 0}),
                     "PTS complex block preserves packed height and NSWE");
  failures += expect(matches(bytes, 148, {0x41, 0, 2, 0, 0x1f, 0, 0x38, 0}),
                     "PTS multilayer marker is the sum of layers");
  failures += expect(matches(bytes, 1932, {0, 0, 0xb0, 0xff, 0xb0, 0xff}),
                     "PTS flat block stores both heights in little endian");

  const auto original_l2j = read_bytes(l2j_path);
  auto existing_output_rejected = false;
  try {
    exporter.export_geodata(buffer, "24_18");
  } catch (const std::exception &) {
    existing_output_rejected = true;
  }
  failures += expect(existing_output_rejected,
                     "export refuses to overwrite an existing file pair");
  failures +=
      expect(read_bytes(path) == bytes && read_bytes(l2j_path) == original_l2j,
             "rejected export preserves both existing files");

  for (const auto &existing_name : {"23_20.l2j", "23_20_conv.dat"}) {
    const auto existing_path = output_directory / existing_name;
    {
      std::ofstream existing{existing_path, std::ios::binary};
      existing.put('X');
    }
    auto partial_output_rejected = false;
    try {
      exporter.export_geodata(buffer, "23_20");
    } catch (const std::exception &) {
      partial_output_rejected = true;
    }
    failures += expect(partial_output_rejected,
                       "export refuses to complete a partial file pair");
    failures +=
        expect(read_bytes(existing_path) == std::vector<std::uint8_t>{'X'},
               "rejected export preserves an existing single file");
    std::filesystem::remove(existing_path);
  }

  buffer.set_block_type(2, 0, geodata::BLOCK_MULTILAYER);
  auto malformed_buffer_rejected = false;
  try {
    exporter.export_geodata(buffer, "24_19");
  } catch (const std::exception &) {
    malformed_buffer_rejected = true;
  }
  failures += expect(malformed_buffer_rejected,
                     "empty multilayer column rejects a staged export");
  failures +=
      expect(!std::filesystem::exists(output_directory / "24_19.l2j") &&
                 !std::filesystem::exists(output_directory / "24_19_conv.dat"),
             "failed export publishes neither file");
  failures += expect(
      std::distance(std::filesystem::directory_iterator{output_directory},
                    std::filesystem::directory_iterator{}) == 2,
      "failed export removes its staging directory");

  buffer.set_block_type(2, 0, geodata::BLOCK_SIMPLE);
  buffer.set_block_height(0, 0, 20000);
  auto height_overflow_rejected = false;
  try {
    exporter.export_geodata(buffer, "24_20");
  } catch (const std::runtime_error &error) {
    height_overflow_rejected =
        std::string{error.what()}.find("outside the packed range") !=
        std::string::npos;
  }
  failures += expect(height_overflow_rejected,
                     "packed-height overflow rejects a staged export");
  failures +=
      expect(!std::filesystem::exists(output_directory / "24_20.l2j") &&
                 !std::filesystem::exists(output_directory / "24_20_conv.dat"),
             "height overflow publishes neither file");
  failures += expect(
      std::distance(std::filesystem::directory_iterator{output_directory},
                    std::filesystem::directory_iterator{}) == 2,
      "height overflow removes partial files and staging directory");
  return failures;
}
