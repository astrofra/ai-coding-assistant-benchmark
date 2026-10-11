import java.io.PrintWriter;
import java.time.LocalDate;

/** One coherent clock per frame; deterministic overrides are for comparison only. */
final class FloraClock {
    private static final long epoch = System.currentTimeMillis();
    private static final long origin = System.nanoTime();
    static long time;
    static LocalDate fixedDate;
    static PrintWriter geometry;
    static long now() { return time; }
    static void tick() { time = epoch + (System.nanoTime() - origin) / 1_000_000L; }
    static LocalDate date() { return fixedDate == null ? LocalDate.now() : fixedDate; }
}
