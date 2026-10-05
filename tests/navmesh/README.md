# Offline navmesh verification

No server is started or modified. Build `navmesh_tests` and run CTest first.
The C++ fixture generator requires a fresh output directory (never overwrites):

```powershell
.\build\main\install\navmesh_tests.exe --fixtures 'D:\navmesh-tests\run-1'
```

Download the published test dependency
[org.recast4j:detour:1.5.12](https://repo.maven.apache.org/maven2/org/recast4j/detour/1.5.12/detour-1.5.12.jar)
to a local test directory. Verify SHA-256:
`bcd283f7daf0605136be979bf9322ed90f408784abeabf8fb1df34a76714dac6`.

Using Java 25 (tested), from the repository root:

```powershell
javac -cp 'D:\navmesh-tests\detour-1.5.12.jar' -d 'D:\navmesh-tests\classes' tests/navmesh/VerifyNavmesh.java
java -cp 'D:\navmesh-tests\classes;D:\navmesh-tests\detour-1.5.12.jar' VerifyNavmesh 'D:\navmesh-tests\run-1\floors.navmesh' --fixtures
```

Expected: four tiles, nonzero polygon count, lower and upper floor paths cross
internal seams, but the two overlapping floors are not connected. C++ tests
assert the same reachability before and after serialization. Fixture points
use negative world coordinates to exercise signed tile indices.

To inspect a real region and query a pair in **game XYZ**, use the same six
coordinates in both readers:

```powershell
.\build\main\install\navmesh_tests.exe --inspect 'D:\navmesh-tests\25_19.navmesh' 166136 53528 -4112 166136 53544 -4112
java -cp 'D:\navmesh-tests\classes;D:\navmesh-tests\detour-1.5.12.jar' VerifyNavmesh 'D:\navmesh-tests\25_19.navmesh' 166136 53528 -4112 166136 53544 -4112
```

Both print tile/polygon counts and `reachable=true/false`. Query modes report
reachability, not an expected result assertion. The Java `--fixtures` mode
fails with a nonzero exit if any literal expected result is wrong. Inputs
are transformed to Detour `(x,z,y)`, snapped within 8 horizontal / 32 vertical
world units. A partial path is not counted as reaching the destination.
These tolerances are test rules, not final server actor/height-selection rules.

Format contract: MSET 1, Detour tile version 7, little endian, 64-bit refs,
6 vertices/polygon, world origin `(0,0,0)`, one game unit per nav unit.
Use `new MeshSetReader().read(input, 6)`, **not** its 32-bit entry point.
The outer header is 40 bytes; each tile entry has an 8-byte ref, 4-byte data
length, 4 zero padding bytes, then the Detour blob. No custom header precedes
MSET. The C++ reader deliberately accepts only this generator's ground-only
profile; it is not a general importer for arbitrary navmesh files.

Adjacent region files are loaded through `navmesh::load_world`, which validates
sidecars, profiles, source identities and tile ownership, allocates capacity,
and imports tiles with fresh references. Detour connects the portals. Raw refs
are local to one MSET instance. Keep `XX_YY.navmesh.json` alongside each new
mesh; legacy meshes without it remain standalone only.

`navmesh_world_tests --fixtures NEW_DIRECTORY` additionally writes merged
regional floor, stacked-floor and wall fixtures. Pass that directory as the
third argument of `JavaRouteTests` to check the same merged MSET through the
pinned recast4j runner. `navmesh_world_probe NEW_DIRECTORY mesh...` selects
connected real surfaces and emits reproducible seam/cross-region cases; it
never changes geometry or inserts links.

Real P542 acceptance on 2026-10-04 used `22_22`, `22_21`, `23_21`: two seam
queries in both directions, a three-region query in both directions, and the
same endpoints without the middle region. Six complete queries reached with
validated segments; the missing-middle query remained partial. L2J `22_22`
regeneration matched the existing file byte-for-byte. Local reproducible cases,
source hashes, runner reports and the PowerShell harness are under
`.local/analysis/navmesh-world-seams-20261003/` (not distributed client data).
