import java.nio.file.*;
import java.util.*;
import org.recast4j.detour.*;

public final class JavaRouteTests {
    static void require(boolean v, String s) { if(!v) throw new AssertionError(s); }
    static int failures;
    interface Test { void run() throws Exception; }
    static void test(String name, Test f) {
        try { f.run(); System.out.println("PASS " + name); }
        catch(Throwable t) { ++failures;System.err.println("FAIL " + name + ": " + t); }
    }
    static NavMesh portalDetailMesh() {
        var data=new MeshData();data.header=new MeshHeader();
        data.header.polyCount=1;data.header.vertCount=3;data.header.maxLinkCount=3;
        data.header.detailMeshCount=1;data.header.detailVertCount=1;data.header.detailTriCount=2;
        data.header.bmax[0]=100;data.header.bmax[1]=12;data.header.bmax[2]=100;
        data.verts=new float[]{0,0,0, 0,0,100, 100,0,0};
        var poly=new Poly(0,6);poly.vertCount=3;poly.flags=1;
        poly.verts[0]=0;poly.verts[1]=1;poly.verts[2]=2;data.polys=new Poly[]{poly};
        var detail=new PolyDetail();detail.vertCount=1;detail.triCount=2;
        data.detailMeshes=new PolyDetail[]{detail};data.detailVerts=new float[]{50,12,50};
        // As in the real tile, the detail triangles cover the portal but its
        // subdivided edges are not flagged as boundary edges.
        data.detailTris=new int[]{0,1,3,1, 0,3,2,16};
        return new NavMesh(data,6,0);
    }
    @SuppressWarnings("unchecked")
    public static void main(String[] args) throws Exception {
        Path mesh=Path.of(args[0]);
        Path fixtures=Path.of(args[1]);
        if(args.length>2) {
            Path world=Path.of(args[2]);
            test("C++ merged regional tiles route in both directions", () -> {
                double[] a={32512,256,1},b={33024,256,1};
                for(var pair:List.of(new double[][]{a,b},new double[][]{b,a})) {
                    var r=NavmeshRouteRunner.run(world.resolve("world.navmesh"),"r","world",pair[0],pair[1],8,32);
                    require(r.get("status").equals("reached") && Boolean.TRUE.equals(r.get("segments_valid")),"merged seam route failed");
                }
            });
            test("C++ merged floors stay separate and wall stays blocked", () -> {
                for(double z:new double[]{1,129}) {
                    var r=NavmeshRouteRunner.run(world.resolve("world-floors.navmesh"),"r","floor",new double[]{32512,256,z},new double[]{33024,256,z},8,32);
                    require(r.get("status").equals("reached"),"merged same floor lost");
                }
                var separate=NavmeshRouteRunner.run(world.resolve("world-floors.navmesh"),"r","separate",new double[]{32512,256,1},new double[]{33024,256,129},8,32);
                require(!separate.get("status").equals("reached"),"merged floors connected");
                var wall=NavmeshRouteRunner.run(world.resolve("world-wall.navmesh"),"r","wall",new double[]{32512,256,1},new double[]{33024,256,1},8,32);
                require(!wall.get("status").equals("reached"),"merged wall connected");
            });
        }
        test("portal samples use owning detail triangles when edge flags omit the portal", () -> {
            var nav=portalDetailMesh();var q=new NavMeshQuery(nav);long ref=nav.getPolyRefBase(nav.getTile(0));
            var straight=List.of(new StraightPathItem(new float[]{25,0,75},1,ref),
                                 new StraightPathItem(new float[]{75,0,25},2,0));
            var samples=NavmeshRouteRunner.surfacePath(q,straight,List.of(ref));
            require(samples.size()>3,"portal has no intermediate surface samples");
            require(samples.getFirst().equals(List.of(25.0,75.0,6.0)),"wrong portal start height or XY");
            require(samples.getLast().equals(List.of(75.0,25.0,6.0)),"wrong portal end height or XY");
            for(var p:samples) {
                require(Math.abs(p.get(0)+p.get(1)-100)<0.001,"sample left its polygon portal");
                double expected=12-Math.abs(p.get(0)-50)*0.24;
                require(Math.abs(p.get(2)-expected)<0.001,"lost detail relief on portal");
            }
        });
        test("surface samples beyond the owning polygon are still rejected", () -> {
            var nav=portalDetailMesh();var q=new NavMeshQuery(nav);long ref=nav.getPolyRefBase(nav.getTile(0));
            boolean rejected=false;
            try { NavmeshRouteRunner.surface(q,ref,new float[]{51,0,51}); }
            catch(IllegalArgumentException expected) { rejected=true; }
            require(rejected,"out-of-polygon sample accepted");
        });
        test("same lower floor reaches exact target with sampled heights", () -> {
            var r=NavmeshRouteRunner.run(mesh,"r","lower",new double[]{-1920,-1920,1},new double[]{-128,-128,1},8,32);
            require(r.get("status").equals("reached"),"expected reached");
            var samples=(List<List<Double>>)r.get("final_path");
            require(samples.size()>3,"expected surface samples, not two-point chord");
            for(int i=1;i<samples.size();i++) {
                var a=samples.get(i-1);var b=samples.get(i);
                require(Math.hypot(b.get(0)-a.get(0),b.get(1)-a.get(1))<=8.01,"sampling gap");
                require(Math.abs(b.get(2)-1)<0.01,"wrong floor");
            }
            require(Boolean.TRUE.equals(r.get("segments_valid")),"surface validation unavailable");
        });
        test("disconnected floors produce partial, not reached", () -> {
            var r=NavmeshRouteRunner.run(mesh,"r","floors",new double[]{-1920,-1920,1},new double[]{-128,-128,129},8,32);
            require(r.get("status").equals("partial"),"unreachable floor must not be successful");
        });
        test("nearest polygon outside snap radius is rejected", () -> {
            var r=NavmeshRouteRunner.run(mesh,"r","snap",new double[]{-1920,-1920,65},new double[]{-128,-128,1},8,32);
            require(r.get("status").equals("invalid_endpoint"),"middle of air is not an endpoint");
        });
        test("wall detour follows corridor instead of drawing through obstacle", () -> {
            var r=NavmeshRouteRunner.run(fixtures.resolve("wall.navmesh"),"r","wall",new double[]{64,96,1},new double[]{192,96,1},8,32);
            require(r.get("status").equals("reached"),"wall detour missing");
            var points=(List<List<Double>>)r.get("final_path");
            require(points.stream().anyMatch(p->p.get(1)>144),"route cut through wall");
        });
        test("solid wall agrees with native unreachable result", () -> {
            var r=NavmeshRouteRunner.run(fixtures.resolve("blocked.navmesh"),"r","blocked",new double[]{64,96,1},new double[]{192,96,1},8,32);
            require(r.get("status").equals("partial"),"blocked route silently successful");
        });
        test("stairs retain detail heights and intermediate samples", () -> {
            var r=NavmeshRouteRunner.run(fixtures.resolve("stairs.navmesh"),"r","stairs",new double[]{64,96,1},new double[]{192,96,25},8,32);
            require(r.get("status").equals("reached"),"stair route missing");
            var points=(List<List<Double>>)r.get("final_path");
            require(points.size()>10 && points.stream().anyMatch(p->p.get(2)>4 && p.get(2)<20),"stair heights collapsed to endpoint chord");
        });
        test("missing and malformed mesh cannot return a route", () -> {
            var missing=mesh.resolveSibling("no-such-route-fixture.navmesh");boolean rejected=false;
            try { NavmeshRouteRunner.run(missing,"r","missing",new double[]{0,0,0},new double[]{1,1,0},8,32); }
            catch(Exception expected) { rejected=true; }
            require(rejected,"missing input silently accepted");
            Path bad=Files.createTempFile("route-bad-", ".navmesh");
            try {
                Files.writeString(bad,"not a mesh");rejected=false;
                try { NavmeshRouteRunner.run(bad,"r","bad",new double[]{0,0,0},new double[]{1,1,0},8,32); }
                catch(Exception expected) { rejected=true; }
                require(rejected,"invalid profile silently accepted");
            } finally { Files.delete(bad); }
        });
        System.out.println("Java route failures: " + failures);
        if(failures!=0) System.exit(1);
    }
}
