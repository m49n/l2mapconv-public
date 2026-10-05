import java.io.BufferedInputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Locale;
import org.recast4j.detour.*;
import org.recast4j.detour.io.MeshSetReader;

/** Offline reader/query acceptance test. Never connects to or modifies a server. */
public final class VerifyNavmesh {
    static float[] nav(float x, float y, float z) { return new float[]{x,z,y}; }
    static boolean near(float[] a, float[] b) {
        return Math.hypot(a[0]-b[0], a[2]-b[2]) <= 8 && Math.abs(a[1]-b[1]) <= 32;
    }
    static boolean reachable(NavMesh mesh, float[] a, float[] b) {
        var q = new NavMeshQuery(mesh);
        var filter = new DefaultQueryFilter();
        var ra = q.findNearestPoly(a, new float[]{8,32,8}, filter);
        var rb = q.findNearestPoly(b, new float[]{8,32,8}, filter);
        if (!ra.succeeded() || !rb.succeeded()) throw new AssertionError("Nearest polygon query failed");
        var start = ra.result; var end = rb.result;
        if (start.getNearestRef()==0 || end.getNearestRef()==0) return false;
        if (!near(a,start.getNearestPos()) || !near(b,end.getNearestPos())) return false;
        var path = q.findPath(start.getNearestRef(),end.getNearestRef(),
                             start.getNearestPos(),end.getNearestPos(),filter);
        if (path.failed()) throw new AssertionError("Path query failed: "+path.status);
        return !path.result.isEmpty() && path.result.getLast()==end.getNearestRef();
    }
    static void check(boolean value, String reason) {
        if (!value) throw new AssertionError(reason);
    }
    public static void main(String[] args) throws Exception {
        Locale.setDefault(Locale.ROOT);
        if(args.length!=1 && args.length!=2 && args.length!=7)
            throw new IllegalArgumentException("file.navmesh [--fixtures | x1 y1 z1 x2 y2 z2]");
        NavMesh mesh;
        try(var in = new BufferedInputStream(Files.newInputStream(Path.of(args[0])))) {
            mesh = new MeshSetReader().read(in,6);
        }
        int tiles=0,polygons=0;
        for(int i=0;i<mesh.getMaxTiles();i++) {
            var tile=mesh.getTile(i);
            if(tile!=null && tile.data!=null && tile.data.header!=null) {
                tiles++; polygons+=tile.data.header.polyCount;
            }
        }
        check(tiles>0 && polygons>0,"Empty mesh");
        System.out.printf("tiles=%d polygons=%d%n",tiles,polygons);
        if(args.length==2) {
            check(args[1].equals("--fixtures"),"Unknown option");
            check(tiles==4,"Expected four negative-coordinate tiles");
            check(reachable(mesh,nav(-1920,-1920,1),nav(-128,-128,1)),"Lower floor seam path");
            check(reachable(mesh,nav(-1920,-1920,129),nav(-128,-128,129)),"Upper floor seam path");
            check(!reachable(mesh,nav(-1920,-1920,1),nav(-128,-128,129)),"Floors must stay disconnected");
            System.out.println("PASS lower/upper seam paths; disconnected floors");
        } else if(args.length==7) {
            var a=nav(Float.parseFloat(args[1]),Float.parseFloat(args[2]),Float.parseFloat(args[3]));
            var b=nav(Float.parseFloat(args[4]),Float.parseFloat(args[5]),Float.parseFloat(args[6]));
            System.out.println("reachable="+reachable(mesh,a,b));
        }
    }
}
