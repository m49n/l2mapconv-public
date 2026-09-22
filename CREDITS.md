# Credits

## Project history and contributors

- Michael Spencer (`Bigcheese`) is credited by the project MIT license (2011)
  and authored the historical L2PackageTools reference.
- `m49n` maintains the original public repository used as the publication
  target.
- Roman Madyanov is credited by the project MIT license (2020) and authored the
  original implementation history plus most of the later C++ and L2J geodata
  builder work.
- Sonereign maintained the upgraded fork and contributed its Windows CI and
  later compatibility/build updates.
- Not-a-Bug Won't Fix contributed dependency, CMake, CI, and maintenance
  updates to the preserved fork history.

The Sonereign fork was integrated by fast-forwarding its Git history. Its
individual commits and author identities remain intact rather than being
copied or squashed into a replacement commit.

## P542 compatibility references

- [UEViewer](https://github.com/gildor2/UEViewer) by Konstantin Nosov (Gildor),
  MIT licensed, was used as an independent Unreal package/version and object
  layout reference.
- [L2 Geodata Toolkit](https://github.com/StylerHub/l2-geodata-toolkit) by
  Alexander Sikachev / StylerHub was used as a behavioral reference for P542
  package parsing and as a source of later validation ideas for terrain,
  collision, layers, NSWE, diffing, and atomic output.
- [m49n/l2-geodata-toolkit](https://github.com/m49n/l2-geodata-toolkit) is the
  publication fork that was supplied for comparison; its PTS format notes and
  round-trip behavior informed independent validation of the C++ PTS writer.
- [GeodataConverter](https://github.com/MrKirill1232/GeodataConverter) by
  MrKirill1232, with the PTS writer attributed in source to Index, was
  consulted as a second format reference via the
  [m49n fork](https://github.com/m49n/GeodataConverter).

No Python source from L2 Geodata Toolkit is copied into this project and it is
not a runtime dependency. Its repository currently contains no explicit
license file, so useful behavior is being reimplemented independently in C++.
No Java source from GeodataConverter is copied into this project either.

## Bundled dependencies

- [Recast Navigation](https://github.com/recastnavigation/recastnavigation)
- [cxxopts](https://github.com/jarro2783/cxxopts)
- [GLEW CMake](https://github.com/Perlmint/glew-cmake)
- [GLFW](https://github.com/glfw/glfw)
- [GLM](https://github.com/g-truc/glm)
- [Dear ImGui](https://github.com/ocornut/imgui)
- [stb](https://github.com/nothings/stb)

Each dependency remains at the Git submodule revision recorded by this
repository and retains its own upstream license.

## Historical implementation references

- [L2PackageTools](https://github.com/Bigcheese/L2PackageTools)
- [l2mapper](https://github.com/justgos/l2mapper)
- [UT Package Tool](https://www.acordero.org/projects/unreal-tournament-package-tool)
- Unreal Tournament 2003
