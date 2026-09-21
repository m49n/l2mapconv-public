# l2mapconv

<p align="center">
    <img src="assets/cruma.png" width="400">
    <img src="assets/toi.png" width="400">
</p>

Lineage II map previewer and geodata builder.

- Supported geodata generation clients: C1, HF.
- Supported geodata format: L2J.
- Experimental map preview: Essence P542, verified with Samurai Crow EU maps,
  including dynamic streaming around `22_22`.

## Features

- Map and geodata preview with a discoverable map catalog.
- Collisionless six-axis free camera with geometry filters.
- Manual terrain selection plus automatic full-detail streaming for the current
  map and, optionally, its immediate neighbours.
- L2J geodata building.

## Usage

Double-click `l2mapconv.exe` to reopen the most recent valid client. On first
launch, choose the client's `sam` directory. Desktop settings are stored under
`%LOCALAPPDATA%\l2mapconv\settings.ini`; the client remains read-only.

The `Client` window shows the active client root, a `Browse...` button, and up
to ten recent client roots (newest first). Choosing a different root validates
its map catalog, shuts the current loading/rendering session down cleanly, and
then starts a fresh session. Desktop startup prefers `22_22` and otherwise
opens the first sorted numeric map available in `Maps`.

Command-line preview and build invocations remain available. In particular,
`--build` never opens a folder picker and does not change the recent-client
history.

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

The geometry-only profile has been verified locally with `22_22` from the
Lineage II Essence Samurai Crow EU P542 client:

```powershell
.\build\preview-core\install\l2mapconv.exe --preview --log-level 4 --client-root "D:\line\clients\Lineage II - Essence - Samurai Crow - EU-P542\sam" -- 22_22
```

Controls:

- Hold right mouse button and move the mouse to look around.
- `WASD` moves horizontally.
- `Space` and `Left Ctrl` move vertically.
- `Left Shift` accelerates and `Left Alt` slows movement.
- `M` toggles wireframe rendering.
- The Maps window lists every numeric `Maps/*.unr` region found in the client.
  Individual checkboxes pin terrain-only regions; `Select All` and
  `Clear Manual` make whole-world terrain inspection practical without loading
  every static mesh. Each cell suffix reports its state: `-` unloaded,
  `Q` queued, `L` loading, `T` terrain resident, `D` detail resident, and
  `!` failed.
- The Rendering window shows the map under the camera and controls automatic
  full-detail loading for the current map. `Include +1 neighbors` independently
  enables or disables the surrounding 3x3 detail neighbourhood. It also shows
  world coordinates, camera speed, mouse sensitivity, and independent
  visibility switches for terrain, static meshes, CSG, and Blocking Volumes.
- The Maps summary reports manual selections, terrain/detail residents, and
  queued/loading/failed work while the background loader keeps the viewer
  responsive.

For a first whole-world pass, use the texture-disabled `preview-core` build,
press `Select All`, and leave automatic current-map loading enabled. All
selected maps are then kept as terrain-only geometry while the current map (and
the optional `+1` ring) retains full detail. The texture-enabled profile remains
useful for targeted smoke tests, but is not the recommended way to load the
whole world.

Preview treats the client as read-only input. Debug logging does not dump
decrypted packages, and automatic ImGui settings persistence is disabled so
the current working directory is not modified. The log reports actor, vertex,
and triangle counts for terrain, static meshes, CSG, and Blocking Volumes.
Unsupported or missing P542 texture references are logged and fall back to
surface colors; use the geometry-only profile when inspecting collision
geometry.

The Geodata `Reset` and `Build` buttons are disabled in streamed preview mode:
the visible scene may contain terrain-only regions and is not a complete
generation input. The command-line `--build` workflow is unchanged and remains
the only complete-map generation path.

P542 geodata generation has not been validated yet. The current P542 scope is
map inspection, not a claim that generated geodata is ready for a live server.
Texture-complete terrain preview and high-resolution radar-map export are
separate future work, outside this milestone.

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
