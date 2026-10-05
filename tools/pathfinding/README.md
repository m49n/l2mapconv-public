# Pathfinding Lab

The Lab compares navigation data using explicit, immutable inputs (L2J one region,
Navmesh one region or a validated regional set).
The local desktop package can include a prepared offline Java backend snapshot.
It never starts or modifies GameServer. Interactive timing
is diagnostic latency, not a statistically meaningful throughput benchmark.

## Case schema 1

Paths are absolute local paths. Replace the example hashes with actual SHA-256
digests (lowercase hexadecimal); sizes are bytes. Game coordinates are XYZ with
Z up. Both endpoints must belong to the named region. `confirmed_z` is the floor
the user selected, not a height inferred from a backend result. Either `l2j`
or `navmesh` may be `null`; at least one complete identity is required.
An optional endpoint `backend_z` object records each backend's picked height
for the same floor (keys `l2j` / `navmesh`). Old cases without it retain their
explicit common height. `search_settings` is optional in old cases; new saves
record it explicitly:

```json
{"server_budget_ms":100,"timeout_ms":5000,"nav_max_nodes":65535,"l2j_max_window_cells":512}
```

```json
{
  "schema": 1,
  "case_id": "corner-forward",
  "map": "25_19",
  "a": {"requested": [166136, 53528, -4112], "confirmed_z": -4112},
  "b": {"requested": [166136, 53544, -4112], "confirmed_z": -4112},
  "l2j": {"path": "D:/navigation/25_19.l2j", "sha256": "<64 hex characters>", "size": 0},
  "navmesh": {"path": "D:/navigation/25_19.navmesh", "sha256": "<64 hex characters>", "size": 0},
  "snap_horizontal": 8,
  "snap_vertical": 32
}
```

Loading a case never silently adopts changed data. Saving creates a new file;
replacing source files or previous reports is not supported. Unknown metrics
and unavailable resolved endpoints are `null`, never fabricated zeros.

Backend configuration is separate from navigation cases: a case cannot choose
an executable. Java, classpath and configuration paths come from an explicitly
selected local backend profile or the bundled profile beside the executable.

## Viewer

Open **Pathfinding Lab** (collapsed by default). Map starts with the region under
the camera. **Current** explicitly switches to the camera's region and clears
previous navigation inputs/points; flying alone never switches loaded data.
Select its `.l2j`, `.navmesh`, or both, then **Load navigation data**. **Clear**
beside a file removes it from the selection, not from disk. This is independent of the
Maps checkboxes and streamed scene. The bundled `pathfinding-backend/profile.json`
is selected automatically. **Backend settings (advanced)** retains manual selection.
Missing Java/backends produce `backend_unavailable`, not a fake route.

Enable **Top view**, use **Center on data**, the wheel to zoom and the middle
mouse button to pan. **Set A**, click the scene, and explicitly choose a floor
when more than one source/layer exists at that XY. A automatically arms B.
Unique corresponding L2J/Navmesh surfaces within 32 Z units are offered as one
floor; each engine receives its own picked Z. Multiple possible matches remain
explicit choices. Nearby polygons on the same surface are deduplicated.
The L2J Lab view/picker follows the packaged server's flat-block 16-unit height
mask; complex/multilayer heights remain 8-unit packed values. Source `.l2j`
bytes are not changed. Old saved explicit points remain unchanged on replay.
The Z slice filters the picture and pick candidates, not pathfinding. Escape
cancels picking; dragging and clicking a UI control cannot place points.
Leaving Top view restores the previous flight camera exactly.

**Find path** queries only the loaded formats, sequentially in a background job.
Orange is L2J, cyan is Navmesh; thin lines are raw L2J evidence. Changed points,
data or backend profile make old lines grey/stale. Cells show standard L2J
NSWE; polygon outlines describe navigation surfaces, not character footsteps.
Navmesh outlines cover all polygons in the visible area/Z slice, without a
first-N cutoff. L2J is a colored coverage overview at distant scales, with exact
16-unit cells and NSWE directions on zoom. Coarse bins summarize occupied cells
and may contain empty subcells; they are not a movement validator. All layers
within the Z slice are projected, and mixed masks are amber. Coarse full-region
layers are cached across camera movement. Z-clipped paths are labelled.
**Swap** checks the reverse direction. **Clear** never deletes files.

**Results** stays above the scrolling controls: one colored status card per
loaded backend, search milliseconds, final-point count and XYZ path length. The
editable **Server reference (ms)** is diagnostic only: a result can exceed it
and still complete. The result card shows the search/reference ratio, excluding
Java startup and file loading. **Details**
shows the diagnostic, snapped coordinates and separate startup/load/identity/
wall timers; these overlapping scopes are not summed. **Open report/folder**
opens saved evidence. **Export CSV** writes the final game XYZ polyline to a new
file. A polyline can still have `layer_mismatch` or another warning: its presence
does not mean both backends reached the requested floor. CSV is geometry, not
a substitute for the status/evidence in `report.json`.

**Save case** creates a new JSON file. **Load case** checks saved input hashes
in the background. If data changed, explicitly adopt current files to start a
new comparison; the saved case and old reports are never rewritten.

Saving after a current completed query also records `expected_backends`:
an optional object mapping `l2j` and `navmesh` to backend-manifest SHA-256.
Replays reject changed backend binaries/classes/configuration before querying.
Cases saved before a query have no backend baseline; the panel labels this
explicitly. **Accept current backends for a NEW comparison** deliberately clears
the baseline and makes previous results stale. Run again and save a new case to
record the new identities. A backend that could not run has no proven baseline.
Source identities are shown in the panel; adjacent generation reports/settings
are not imported automatically in this first stage.

Maps' arrow button flies to the **clicked** region even when unchecked. It
does not change selections, auto-load flags or A/B. In Top view it moves the
view center without changing zoom or the navigation dataset.

## Backend preparation (explicit local dependencies)

Requires a JDK (validated with Java 25) and the published
`org.recast4j:detour:1.5.12` JAR. Expected SHA-256:
`BCD283F7DAF0605136BE979BF9322ED90F408784ABEABF8FB1DF34A76714DAC6`.
The public Windows ZIP already includes the Navmesh runner, the pinned JAR,
and a Java runtime. Keep `pathfinding-backend` beside the application; no
manual profile is needed for Navmesh.

For a source checkout, compile the public runner into a new directory using
your own JDK and the pinned JAR:

```powershell
& 'D:/jdk-25/bin/javac.exe' -encoding UTF-8 `
  -cp 'D:/dependencies/detour-1.5.12.jar' -d 'D:/lab/backend-new/classes' `
  tools/pathfinding/NavmeshRouteRunner.java tools/pathfinding/BenchmarkProgress.java
```

The external server classes must expose
`org.mmocore.gameserver.geoengine.benchmark.GeoRouteBenchmark` with the compatible
20-column case CSV and run/sample/evidence/summary JSONL protocol. Include its
complete runtime classpath. Do not supply a GameServer startup command.
L2J route queries require an independently prepared compatible offline
backend. The public source tree and ZIP do not contain server classes,
configuration, source patchers or server-specific preparation tools.

Profile schema 1 has `java` (executable), `legacy_classpath` (array),
`legacy_config` (file), `nav_runner_directory` (compiled classes), and
`detour_jar`. Empty paths disable the corresponding backend. The profile is a
trusted local executable configuration; cases cannot choose executables.
`legacy_experimental_limits` is true only for a snapshot with the audited
offline overrides. Old profiles remain usable, but the UI/report explicitly
say that the legacy engine's original limits apply.
Paths in a profile file can be relative to that file's directory; immutable job
requests contain resolved absolute paths. Java is never guessed from PATH.

Example Navmesh-only profile (save it beside the compiled `classes` directory):

```json
{
  "schema": 1,
  "java": "D:/jdk-25/bin/java.exe",
  "legacy_classpath": [],
  "legacy_config": "",
  "nav_runner_directory": "classes",
  "detour_jar": "D:/dependencies/detour-1.5.12.jar",
  "legacy_experimental_limits": false
}
```

The supplied paths are examples, not a server configuration. Preserve license
notices when distributing dependencies. Public release packages contain no
navigation datasets, saved cases, benchmark reports or user settings.

## Experimental search limits

- Server reference: default **100 ms**, editable 1..30000; never stops a search.
- Search timeout: default **5000 ms**, editable 1..30000; enforced in each engine's
  search, independently from process startup/loading. Deadlines are cooperative
  at search steps; allocation or one step can overshoot, and actual time is shown.
- Navmesh expansion cap: default **65,535**, editable 1..1,000,000 expanded A*
  nodes. This is not a heap or allocated-node count.
- L2J window: default **512 cells**, editable 64..2048 in steps of 32. A cell is
  16 world units; this is the search window side, **not a maximum path length**.
  Large-distance queries may need a larger window as well as more time.

`search_timeout`, `window_limit` and Navmesh `node_limit` are resource limits,
not proof of no path. Instrumented L2J open-list exhaustion is `no_path`;
uninstrumented null remains `unknown_legacy`. Timed L2J classification/counters
come from the measured sample, not its separate untimed geometry replay. If the
timed run hits a limit, geometry from a later successful replay is not presented
as its route; raw evidence remains in `legacy.jsonl`. These are offline controls,
not changes to the running server. Searches still cover one loaded square only.

## CLI replay and reports

UI and CLI share the same worker; no separate query implementation:

```powershell
./dist/l2mapconv.exe --pathfinding-case D:/lab/corner.json `
  --output D:/lab/new-run-001
```

The CLI also uses the bundled profile by default; pass
`--backend-profile D:/lab/backend-new/profile.json` before `--output` to override.
Output must be a new absolute directory outside input and backend directories.
Alternatively create that directory with a new `request.json` and invoke
`--pathfinding-job D:/lab/new-run-001`. Request schema:

```json
{"schema":1,"kind":"pathfinding","job_id":"new-run-001",
 "route":{"schema":1,"...":"complete case object above"},
 "profile":{"schema":1,"...":"complete backend profile object"}}
```

See the request generated by CLI replay for an executable example. Job ID must
match the directory name. Worker writes an exclusive `started.json` claim,
atomic `status.json`, immutable `report.json`, backend logs/evidence, and hashed
backend artifact manifests. `request_sha256` identifies the exact request;
source SHA-256 and backend manifest SHA accompany the results. Inputs and
backend artifacts are rechecked after querying. Reusing a directory fails.
Create `cancel.request` in that job directory to request cancellation. Only
owned processes/descendants are stopped; unrelated Java processes are untouched.

Exit codes: 0 = valid query outcomes (including partial/no path/layer mismatch),
2 = invalid startup/request, 3 = all requested backends failed/hit limits, 130 = cancellation.
Per-backend statuses are authoritative; one backend failure preserves the
other's result. Limits: 60 seconds per backend, 120 seconds per job, cancellation
grace 5 seconds in the viewer. Resource failures are not `no_path`.
The viewer's `timeout.request` marker preserves `resource_limit` on cooperative
shutdown; it is distinct from user cancellation.

`reached` requires target and floor evidence, not merely an API success flag.
Floor tolerance is 4 game units; nearest-poly snapping (8 XY / 32 Z) is a
separate limit, not permission to substitute floors. `unknown_legacy` means a
legacy null has no proven cause. Missing exact resolved-height evidence cannot
be promoted to reached. Native MSET validation precedes Java loading.

Navmesh uses bounded recast4j sliced A* (configurable time/expansions), then funnel/portal
crossings and height samples at most 8 XY units apart on actual detail surfaces.
`corridor` is polygon geometry, not a centerline; this validates the static nav
surface, not client collision, dynamic doors or server movement execution.

Metrics with `_ns` are nanoseconds, `_bytes` are bytes, counts retain their
names. Unsupported metrics stay `null`. Startup, loading, snapping, search,
smoothing and validation are separate. **Find path** uses warmup 0 / iteration 1
/ fork 1: a cold, diagnostic query, not steady-state performance.

**Warmup + measure** runs the selected A -> B repeatedly in one data-bearing JVM
per backend, loading navigation data once. Defaults: 2000 warmups, 100 measured
queries; bounds: 0..10000 and 1..1000. Each query runs the real search again, with
fresh search state (normal L2J buffer reuse is retained), not a cached route.
Warmup and progress I/O are outside reported query timers. A fixed warmup count
does not prove JIT stability; compare batches before drawing conclusions.

Job schema 2 adds `benchmark: {"warmup": 2000, "iterations": 100}`; ordinary jobs
retain schema 1. `metrics.benchmark_samples` contains measured outcomes only.
`metrics.benchmark` reports median (average middle pair), nearest-rank p95/p99,
timer sample counts, status counts and limit counts. Missing search times stay
null (L2J may finish on the direct check without A*). Failures/timeouts are not
silently discarded. The displayed route/status is the last measured Navmesh
query; L2J retains its separately validated evidence replay. All raw records
remain in the job folder. Loading/identity/world preparation/process wall times
are separate from query statistics.

Progress is atomic `benchmark-progress.json`, bound to request/backend/PID;
the controller pins each backend's observed PID. Transient reader/antivirus
locks skip a progress update after bounded retries, never aborting the search.
Cancel terminates only the owned worker/JVM tree. Series safety limits are 600 s
per backend, 1260 s per job; per-search limits are unchanged. Cancelled/timed-out
series do not publish a misleading complete distribution. This is a repeated-route
latency experiment, not a mixed-workload/server TPS benchmark or JMH replacement.
Server movement integration remains separate.
# Multi-region Navmesh

Add `.navmesh` files in the Lab and load the set. New region builds include an
automatically discovered `.navmesh.json` sidecar; old files without it remain
single-region inputs. No manual JSON selection is required. Common source
packages, generator/grid profiles and tile ownership are checked before Detour
reconnects the tiles. Cases with region sets use schema 2; schema 1 is retained.

Workers save a private `world.navmesh` beside the report. Original mesh and
sidecar identities, the merged identity, `prepare_ns`, `merged_load_ns` (same
scope as `load_ns`) and `visited_regions` are reported separately from
`search_ns`. Visited regions are computed from final path segments, including
crossings between sparse samples. L2J remains single-region: unsupported
endpoints produce `unsupported_scope`, not `no_path`.
