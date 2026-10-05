import java.nio.file.*;

/** Exercises the public offline runner boundary, including exclusive output. */
public final class JavaLimitsTests {
    static void require(boolean condition, String message) {
        if (!condition) throw new AssertionError(message);
    }
    static String run(Path mesh, int timeout, int nodes) throws Exception {
        Path directory = Files.createTempDirectory("navmesh-limits-test-");
        Path output = directory.resolve("result.json");
        try {
            NavmeshRouteRunner.main(new String[]{"--mesh", mesh.toString(),
                "--request-id", "limits", "--case-id", "wall",
                "--from", "64,96,1", "--to", "192,96,1",
                "--snap-horizontal", "8", "--snap-vertical", "32",
                "--search-timeout-ms", Integer.toString(timeout),
                "--max-nodes", Integer.toString(nodes), "--output", output.toString()});
            return Files.readString(output);
        } finally {
            Files.deleteIfExists(output);
            Files.delete(directory);
        }
    }
    public static void main(String[] args) throws Exception {
        Path mesh = Path.of(args[0]);
        String limited;
        try { limited = run(mesh, 5000, 1); }
        catch (IllegalArgumentException failure) {
            throw new AssertionError("Experimental node/time controls must be accepted by the runner", failure);
        }
        require(limited.contains("\"status\":\"resource_limit\""), "node cap must not become no_path/partial");
        require(limited.contains("\"limit_reason\":\"node_limit\""), "node stop needs exact reason");
        require(limited.contains("\"visited_nodes\":1"), "one-node budget must expand one node");
        require(limited.contains("\"smoothing_ns\":null"), "node-limited search must not run smoothing");
        String reached = run(mesh, 5000, 65535);
        require(reached.contains("\"status\":\"reached\""), "larger node budget must find wall detour");
        require(reached.contains("\"search_timeout_ms\":5000"), "effective time budget must be reported");
        for (int[] bad : new int[][]{{0,1}, {30001,1}, {1,0}, {1,1000001}}) {
            boolean rejected = false;
            try { run(mesh, bad[0], bad[1]); }
            catch (IllegalArgumentException expected) { rejected = true; }
            require(rejected, "out-of-range experimental budget accepted");
        }
        System.out.println("PASS Java search limits and wall detour");
    }
}
