#include "pch.h"

#include "Decryptor.h"

namespace unreal {

static constexpr std::streamsize LINEAGE_SIZE = 22;
static constexpr std::streamsize VERSION_SIZE = 6;

// "Lineage2Ver" string
static constexpr std::array<char, LINEAGE_SIZE> LINEAGE_HEADER = {
    0x4C, 0x00, 0x69, 0x00, 0x6E, 0x00, 0x65, 0x00, 0x61, 0x00, 0x67,
    0x00, 0x65, 0x00, 0x32, 0x00, 0x56, 0x00, 0x65, 0x00, 0x72, 0x00};

void Decryptor::decrypt(const std::filesystem::path &path,
                        std::ostream &output) {

  std::ifstream input{path, std::ios::binary};
  const auto version = extract_version(input);

  if (!version.has_value()) {
    ASSERT(false, "Unreal", "Can't detect Lineage 2 encryption version");
    return;
  }

  switch (version.value()) {
  case 111: {
    decrypt_v111(input, output);
    break;
  }
  case 121: {
    decrypt_v121(input, output, path);
    break;
  }
  default: {
    ASSERT(false, "Unreal",
           "Unsupported Linage 2 encryption version: " << version.value());
  }
  }
}

auto Decryptor::extract_version(std::istream &input) -> std::optional<int> {
  std::array<char, LINEAGE_SIZE> lineage_buffer{};
  std::array<char, VERSION_SIZE> version_buffer{};
  input.read(lineage_buffer.data(), LINEAGE_SIZE);
  input.read(version_buffer.data(), VERSION_SIZE);

  if (lineage_buffer != LINEAGE_HEADER) {
    return {};
  }

  std::string version_string;
  version_string += version_buffer[0];
  version_string += version_buffer[2];
  version_string += version_buffer[4];

  std::istringstream version_stream(version_string);
  auto version = 0;
  version_stream >> version;

  return version;
}

void Decryptor::decrypt_xor(std::istream &input, std::ostream &output,
                            int key) {

  std::istreambuf_iterator input_iterator{input};
  std::ostreambuf_iterator output_iterator{output};
  std::istreambuf_iterator<char> end;

  for (auto it = input_iterator; it != end; ++it) {
    *(output_iterator++) = *it ^ key;
  }
}

void Decryptor::decrypt_v111(std::istream &input, std::ostream &output) {
  decrypt_xor(input, output, 0xAC);
}

void Decryptor::decrypt_v121(std::istream &input, std::ostream &output,
                             const std::filesystem::path &path) {

  const auto filename = path.filename().string();
  auto key = 0;

  for (const auto &character : filename) {
    key += std::tolower(character);
  }

  decrypt_xor(input, output, key);
}

} // namespace unreal
