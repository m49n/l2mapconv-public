#include <algorithm>
#include <array>
#include <cstdio>
#include <iostream>
#include <memory>
#include <territory/PathIO.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>

#include <shellapi.h>
#endif
namespace {
struct Image {
  int width{}, height{}, channels{};
  std::unique_ptr<unsigned char, decltype(&stbi_image_free)> data{
      nullptr, stbi_image_free};
  explicit Image(const std::filesystem::path &path) {
    FILE *file = nullptr;
#ifdef _WIN32
    _wfopen_s(&file, path.c_str(), L"rb");
#else
    file = std::fopen(path.c_str(), "rb");
#endif
    if (!file)
      throw std::runtime_error("Cannot open image");
    std::unique_ptr<FILE, decltype(&std::fclose)> owner(file, std::fclose);
    data.reset(stbi_load_from_file(file, &width, &height, &channels, 3));
    if (!data)
      throw std::runtime_error(std::string("PNG decode failed: ") +
                               stbi_failure_reason());
    if (width <= 0 || height <= 0 || width > 16384 || height > 16384 ||
        channels != 3)
      throw std::runtime_error("Expected RGB image up to 16384");
  }
};
} // namespace
int main(int argc, char **argv) {
  try {
    std::vector<std::filesystem::path> args;
#ifdef _WIN32
    (void)argc;
    (void)argv;
    int count = 0;
    auto wide = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!wide)
      throw std::runtime_error("Cannot parse argv");
    struct Free {
      wchar_t **p;
      ~Free() { LocalFree(p); }
    } free{wide};
    for (int n = 1; n < count; ++n)
      args.emplace_back(wide[n]);
#else
    for (int n = 1; n < argc; ++n)
      args.push_back(territory::path_from_utf8(argv[n]));
#endif
    if (args.size() != 1 && args.size() != 2)
      throw std::runtime_error(
          "Usage: territory_png_compare image [second-image]");
    const Image first(args[0]);
    const auto pixels = std::size_t(first.width) * first.height;
    std::array<std::uint64_t, 3> totals{};
    std::array<int, 3> low{255, 255, 255}, high{};
    for (std::size_t n = 0; n < pixels; ++n)
      for (int c = 0; c < 3; ++c) {
        const auto value = first.data.get()[n * 3 + c];
        totals[c] += value;
        low[c] = std::min(low[c], int(value));
        high[c] = std::max(high[c], int(value));
      }
    territory::Json json{
        {"width", first.width},
        {"height", first.height},
        {"channels", first.channels},
        {"bytes", std::filesystem::file_size(args[0])},
        {"minimum", low},
        {"maximum", high},
        {"mean",
         {double(totals[0]) / pixels, double(totals[1]) / pixels,
          double(totals[2]) / pixels}}};
    if (args.size() == 2) {
      const Image second(args[1]);
      if (first.width != second.width || first.height != second.height)
        throw std::runtime_error("Image dimensions differ");
      std::uint64_t total = 0, changed = 0;
      int maximum = 0;
      std::array<int, 4> bounds{first.width, first.height, -1, -1};
      for (std::size_t n = 0; n < pixels; ++n) {
        bool different = false;
        for (int c = 0; c < 3; ++c) {
          const int delta = std::abs(int(first.data.get()[n * 3 + c]) -
                                     int(second.data.get()[n * 3 + c]));
          total += delta;
          maximum = std::max(maximum, delta);
          different |= delta != 0;
        }
        if (different) {
          ++changed;
          const int x = n % first.width, y = n / first.width;
          bounds[0] = std::min(bounds[0], x);
          bounds[1] = std::min(bounds[1], y);
          bounds[2] = std::max(bounds[2], x);
          bounds[3] = std::max(bounds[3], y);
        }
      }
      json["delta"] = {{"maximum", maximum},
                       {"mean", double(total) / (pixels * 3)},
                       {"changed_pixels", changed},
                       {"bounds_xyxy", bounds}};
    }
    std::cout << json.dump() << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
