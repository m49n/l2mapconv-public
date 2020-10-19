#pragma once

#include <vector>

namespace geodata {

enum BlockType {
  BLOCK_SIMPLE,
  BLOCK_COMPLEX,
  BLOCK_MULTILAYER,
};

struct Block {
  int x;
  int y;
  int z;
  BlockType type;
  bool north;
  bool south;
  bool west;
  bool east;
};

struct Geodata {
  std::vector<Block> blocks;
};

} // namespace geodata
