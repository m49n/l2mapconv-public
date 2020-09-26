#include <utils/StreamDump.h>

#include <cstdint>
#include <cstdio>
#include <memory>

namespace utils {

void dump(std::istream &input, int line_count, int line_length) {
  const auto byte_count = line_count * line_length * 2;

  const auto offset = input.tellg();
  auto buffer = std::make_unique<char[]>(byte_count);

  input.seekg(-byte_count / 2, std::ios::cur);
  input.read(buffer.get(), byte_count);

  {
    printf("\n");
    printf("   ");
    printf("\t");

    for (auto i = 0; i < line_length; ++i) {
      printf("%2d ", i);
    }

    printf("\t");
    printf("   ");
    printf("\t");

    for (auto i = 0; i < line_length; ++i) {
      printf("%2d ", i);
    }

    printf("\n");
    printf("\n");
  }

  for (auto line = 0; line < line_count * 2; ++line) {
    if (line == line_count) {
      {
        printf("\n");
        printf("   ");
        printf("\t");

        for (auto i = 0; i < line_length; ++i) {
          printf("%2d ", i);
        }

        printf("\t");
        printf("   ");
        printf("\t");

        for (auto i = 0; i < line_length; ++i) {
          printf("%2d ", i);
        }

        printf("\n");
        printf("\n");
      }
    }

    printf("%3d\t", line - line_count);

    for (auto i = 0; i < line_length; ++i) {
      printf("%02x ",
             static_cast<std::uint8_t>(buffer[(line * line_length) + i]));
    }

    printf("\t");
    printf("%3d\t", line - line_count);

    for (auto i = 0; i < line_length; ++i) {
      const auto &character = buffer[(line * line_length) + i];

      if (isprint(character) != 0) {
        printf(" %c ", character);
      } else {
        printf(" ■ ");
      }
    }

    printf("\t");
    printf("%3d\t", line - line_count);

    for (auto i = 0; i < line_length; ++i) {
      const auto &character = buffer[(line * line_length) + i];

      if (isprint(character) != 0) {
        printf("%c", character);
      } else {
        printf("■");
      }
    }

    printf("\n");
  }

  input.seekg(offset);
}

} // namespace utils
