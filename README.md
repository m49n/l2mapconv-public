# l2mapconv

<p align="center">
    <img src="assets/cruma.png" width="400">
    <img src="assets/toi.png" width="400">
</p>

Lineage II map previewer and geodata builder.

- Supported geodata generation clients: C1, HF.
- Supported geodata format: L2J.
- Experimental map preview: Essence P542, verified with Samurai Crow EU map
  `19_21`.

## Features

- Map and geodata preview.
- Collisionless six-axis free camera with geometry filters.
- L2J geodata building.

## Usage

```sh
l2mapconv.exe --preview/build --client-root <path> -- [maps...]

    --preview          Preview maps
    --build            Build maps (see results in the `output` directory)
    --client-root arg  Path to the Lineage II client
    --log-level arg    Log level (0 - none, 1 - fatal, 2 - error, 3 -
                       warn, 4 - info, 5 - debug, 6 - all) (default: 3)
    --help             Print help
```

> Use `--log-level 4` option to print building progress.

## P542 map preview

The geometry-only profile has been verified locally with `19_21` from the
Lineage II Essence Samurai Crow EU P542 client:

```powershell
.\build\preview-core\install\l2mapconv.exe --preview --log-level 4 --client-root "D:\line\clients\Lineage II - Essence - Samurai Crow - EU-P542\sam" -- 19_21
```

Controls:

- Hold right mouse button and move the mouse to look around.
- `WASD` moves horizontally.
- `Space` and `Left Ctrl` move vertically.
- `Left Shift` accelerates and `Left Alt` slows movement.
- `M` toggles wireframe rendering.
- The Rendering window shows world coordinates, camera speed, mouse
  sensitivity, and independent visibility switches for terrain, static
  meshes, CSG, and Blocking Volumes.

Preview treats the client as read-only input. Debug logging does not dump
decrypted packages, and automatic ImGui settings persistence is disabled so
the current working directory is not modified. The log reports actor, vertex,
and triangle counts for terrain, static meshes, CSG, and Blocking Volumes.
Unsupported or missing P542 texture references are logged and fall back to
surface colors; use the geometry-only profile when inspecting collision
geometry.

P542 geodata generation has not been validated yet. The current P542 scope is
map inspection, not a claim that generated geodata is ready for a live server.

## Project building

Requirements:

- Clang/GCC
- CMake
- Ninja or other build system
- Git to apply the [Recast patch](libs/patches/recast.patch)

### Windows

```powershell
git clone --recurse-submodules -j8 https://github.com/m49n/l2mapconv-public.git
cd l2mapconv-public

cmake -S . -B build/preview-core -G Ninja -D CMAKE_BUILD_TYPE=Release -D L2MAPCONV_LOAD_TEXTURES=OFF -D CMAKE_C_COMPILER="C:/Program Files/LLVM/bin/clang.exe" -D CMAKE_CXX_COMPILER="C:/Program Files/LLVM/bin/clang++.exe"
cmake --build build/preview-core --parallel

cmake -S . -B build/preview-textured -G Ninja -D CMAKE_BUILD_TYPE=Release -D L2MAPCONV_LOAD_TEXTURES=ON -D CMAKE_C_COMPILER="C:/Program Files/LLVM/bin/clang.exe" -D CMAKE_CXX_COMPILER="C:/Program Files/LLVM/bin/clang++.exe"
cmake --build build/preview-textured --parallel
```

### macOS/Linux

```sh
git clone --recurse-submodules -j8 git@github.com:madyanov/l2mapconv-public.git
cd l2mapconv-public
CC=clang CXX=clang++ cmake -S . -B build -G Ninja -D CMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

### CMake Options

- `L2MAPCONV_GEODATA_POST_PROCESSING` — enable geodata compression and cell alignment. Disable to see actual cell positions during development.
- `L2MAPCONV_LOAD_TERRAIN` — disable for faster geodata building during development.
- `L2MAPCONV_LOAD_TEXTURES` — loads textures for supported static meshes and
  BSP surfaces. Keep a separate texture-disabled build as the reliable
  geometry inspection profile.

## Dependencies

- [recast](https://github.com/recastnavigation/recastnavigation)
- [cxxopts](https://github.com/jarro2783/cxxopts)
- [glew](https://github.com/Perlmint/glew-cmake)
- [glfw](https://github.com/glfw/glfw)
- [glm](https://github.com/g-truc/glm)
- [imgui](https://github.com/ocornut/imgui)
- [stb](https://github.com/nothings/stb)

## Credits

See [CREDITS.md](CREDITS.md) for project authors, preserved fork contributors,
dependencies, and the external references used during P542 compatibility work.
