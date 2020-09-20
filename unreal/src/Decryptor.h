#pragma once

#include <filesystem>
#include <iostream>
#include <optional>

namespace unreal {

class Decryptor {
public:
  void decrypt(const std::filesystem::path &path, std::ostream &output);

private:
  auto extract_version(std::istream &input) -> std::optional<int>;
  void decrypt_xor(std::istream &input, std::ostream &output, int key);
  void decrypt_v111(std::istream &input, std::ostream &output);
  void decrypt_v121(std::istream &input, std::ostream &output,
                    const std::filesystem::path &path);
};

} // namespace unreal
