package lab;

import java.io.IOException;
import java.nio.file.*;

/** Small, throttled, atomic progress file for the owned offline JVM. */
public final class BenchmarkProgress {
    private static long lastWrite;
    private static String lastPhase = "";
    private static boolean warned;
    private static String quote(String value) {
        return "\"" + value.replace("\\", "\\\\").replace("\"", "\\\"")
            .replace("\n", "\\n").replace("\r", "\\r").replace("\t", "\\t") + "\"";
    }
    public static void update(String phase, int completed, int total) {
        String file = System.getProperty("l2mapconv.progress", "");
        if (file.isEmpty()) return;
        long now = System.nanoTime();
        if (phase.equals(lastPhase) && completed != total && now - lastWrite < 100_000_000L) return;
        lastWrite = now; lastPhase = phase;
        String json = "{\"request_id\":" + quote(System.getProperty("l2mapconv.requestId", ""))
            + ",\"backend\":" + quote(System.getProperty("l2mapconv.backend", ""))
            + ",\"phase\":" + quote(phase) + ",\"completed\":" + completed
            + ",\"total\":" + total + ",\"pid\":" + ProcessHandle.current().pid() + "}";
        Path target = Path.of(file).toAbsolutePath();
        for (int attempt=0; attempt<3; attempt++) try {
            Path tmp = Files.createTempFile(target.getParent(), ".progress-", ".tmp");
            try {
                Files.writeString(tmp, json);
                Files.move(tmp, target, StandardCopyOption.ATOMIC_MOVE, StandardCopyOption.REPLACE_EXISTING);
            } finally { Files.deleteIfExists(tmp); }
            return;
        } catch (IOException e) {
            // Windows atomic replacement may reject a destination held open
            // by a UI reader/antivirus. Progress must never abort the search.
            if(attempt<2) {
                try { Thread.sleep(5); }
                catch(InterruptedException interrupted) { Thread.currentThread().interrupt(); return; }
            } else if(!warned) {
                warned=true;
                System.err.println("Benchmark progress temporarily unavailable: "+e.getMessage());
            }
        }
    }
}
