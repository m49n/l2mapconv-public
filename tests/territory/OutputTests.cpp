#include "Fixtures.h"
#include "TestSupport.h"
#include <fstream>
#include <territory/FileIdentity.h>
#include <territory/PathIO.h>
#include <territory/PngOutput.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
int output_tests() {
  using namespace territory;
  int failures = 0;
  TestDirectory temp;
  auto path = temp.path() / path_from_utf8("карта.png");
  const std::vector<std::uint8_t> rgb = {1,   2,   3,   127, 128, 129,
                                         253, 254, 255, 40,  50,  60};
  {
    PngOutput out(path, 2, 2);
    out.rows(0, 2, 1, std::span(rgb).first(6));
    out.rows(1, 2, 1, std::span(rgb).subspan(6));
    out.finish({});
  }
  failures += expect(png_has_dimensions(path, 2, 2),
                     "publish complete PNG at Unicode path");
  failures += expect(throws([&] { PngOutput duplicate(path, 2, 2); }),
                     "never replace an existing image");
  if (std::filesystem::is_regular_file(path)) {
    std::ifstream f(path, std::ios::binary);
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(f)), {});
    int w = 0, h = 0, channels = 0;
    auto decoded = stbi_load_from_memory(
        bytes.data(), static_cast<int>(bytes.size()), &w, &h, &channels, 3);
    failures += expect(decoded && w == 2 && h == 2 &&
                           std::equal(rgb.begin(), rgb.end(), decoded),
                       "independent stb decode preserves every RGB byte");
    stbi_image_free(decoded);
    bytes.resize(33);
    auto short_file = temp.path() / "truncated.png";
    std::ofstream short_out(short_file, std::ios::binary);
    short_out.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    short_out.close();
    failures += expect(!png_has_dimensions(short_file, 2, 2),
                       "header-only truncated PNG not complete");
  }
  auto cancelled = temp.path() / "cancelled.png";
  {
    PngOutput out(cancelled, 2, 2);
    out.rows(0, 2, 2, rgb);
    failures += expect(throws([&] { out.finish([] { return true; }); }),
                       "cancel between last row and publication");
  }
  failures += expect(!std::filesystem::exists(cancelled),
                     "cancel leaves no false completed image");
  auto late_cancel = temp.path() / "late-cancel.png";
  {
    int calls = 0;
    PngOutput out(late_cancel, 2, 2);
    out.rows(0, 2, 2, rgb);
    failures +=
        expect(throws([&] { out.finish([&] { return ++calls == 2; }); }),
               "cancel after encoder commit but before rename");
  }
  failures += expect(!std::filesystem::exists(late_cancel),
                     "late cancellation does not publish");
  {
    PngOutput out(temp.path() / "incomplete.png", 2, 2);
    out.rows(0, 2, 1, std::span(rgb).first(6));
    failures +=
        expect(throws([&] { out.finish({}); }), "cannot finish missing rows");
  }
  {
    PngOutput out(temp.path() / "order.png", 2, 2);
    failures +=
        expect(throws([&] { out.rows(1, 2, 1, std::span(rgb).first(6)); }),
               "reject out of order rows");
  }
  {
    PngOutput out(temp.path() / "short.png", 2, 2);
    failures +=
        expect(throws([&] { out.rows(0, 2, 2, std::span(rgb).first(6)); }),
               "reject short row data");
  }
  {
    PngOutput out(temp.path() / "duplicate.png", 2, 2);
    out.rows(0, 2, 1, std::span(rgb).first(6));
    failures +=
        expect(throws([&] { out.rows(0, 2, 1, std::span(rgb).first(6)); }),
               "reject duplicate rows");
  }
  failures +=
      expect(throws([&] {
               PngOutput out(temp.path() / "overflow.png", INT_MAX, INT_MAX);
             }),
             "reject pixel size overflow");
  failures += expect(
      throws([&] { PngOutput out(temp.path() / "absent" / "file.png", 2, 2); }),
      "reject unwritable/nonexistent parent");
  failures += expect(throws([&] {
                       PngOutput out(temp.path() / "fail-write.png", 2, 2,
                                     [](std::string_view stage) {
                                       if (stage == "write")
                                         throw std::runtime_error(
                                             "injected write failure");
                                     });
                       out.rows(0, 2, 2, rgb);
                     }),
                     "write failure propagated");
  failures += expect(throws([&] {
                       PngOutput out(temp.path() / "fail-commit.png", 2, 2,
                                     [](std::string_view stage) {
                                       if (stage == "commit")
                                         throw std::runtime_error(
                                             "injected commit failure");
                                     });
                       out.rows(0, 2, 2, rgb);
                       out.finish({});
                     }),
                     "commit failure propagated");
  auto race = temp.path() / "race.png";
  {
    PngOutput out(race, 2, 2);
    out.rows(0, 2, 2, rgb);
    {
      std::ofstream other(race);
      other << "owned by another writer";
    }
    failures += expect(throws([&] { out.finish({}); }),
                       "no-replace publication survives final-path race");
  }
  std::ifstream r(race);
  std::string value((std::istreambuf_iterator<char>(r)), {});
  failures += expect(value == "owned by another writer",
                     "race does not delete or change foreign final file");
  auto abc = temp.path() / "abc";
  {
    std::ofstream f(abc, std::ios::binary);
    f << "abc";
  }
  const auto id = file_identity(abc);
  failures +=
      expect(id.bytes == 3 && id.sha256 == "ba7816bf8f01cfea414140de5dae2223b00"
                                           "361a396177a9cb410ff61f20015ad",
             "BCrypt SHA-256 known input");
  failures += expect(throws([&] { file_identity(temp.path() / "absent"); }),
                     "missing hash input rejected");
#ifdef _WIN32
  {
    std::ofstream writer(abc, std::ios::binary | std::ios::app);
    failures +=
        expect(throws([&] { file_identity(abc); }),
               "active writer is rejected rather than hashing unstable input");
  }
#endif
  for (auto &entry : std::filesystem::directory_iterator(temp.path()))
    failures += expect(entry.path().extension() != ".tmp",
                       "unfinished owned temporary removed");
  return failures;
}
