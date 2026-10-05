import java.io.IOException;
import java.nio.*;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;
import org.recast4j.detour.*;
import org.recast4j.detour.io.MeshSetReader;
import lab.BenchmarkProgress;

/** Offline recast4j route evidence. Game XYZ at the protocol boundary. */
public final class NavmeshRouteRunner {
    static final int LIMIT=65535, MAX_POINTS=1000000;
    static volatile Object benchmarkSink;
    static void require(boolean v,String message) { if(!v) throw new IllegalArgumentException(message); }
    static List<Double> game(float[] p) { return List.of((double)p[0],(double)p[2],(double)p[1]); }
    static List<Double> source(double[] p) { return List.of(p[0],p[1],p[2]); }
    static float[] nav(double[] p) {
        require(p.length==3,"XYZ required");
        for(double v:p) require(Double.isFinite(v)&&Math.abs(v)<=10000000,"Invalid endpoint coordinate");
        return new float[]{(float)p[0],(float)p[2],(float)p[1]};
    }
    static boolean near(float[] a,float[] b,double h,double v) {
        return Math.hypot(a[0]-b[0],a[2]-b[2])<=h && Math.abs(a[1]-b[1])<=v;
    }
    static Map<String,Object> endpoint(double[] requested,FindNearestPolyResult p,boolean valid) {
        var result=new LinkedHashMap<String,Object>();result.put("requested",source(requested));
        result.put("resolved",game(p.getNearestPos()));result.put("valid",valid);
        result.put("surface_id","nav:"+Long.toUnsignedString(p.getNearestRef()));return result;
    }
    // Profile guard before allocating arrays in MeshSetReader. The application
    // also runs the native full blob validator before starting this worker.
    static NavMesh read(byte[] bytes) throws IOException {
        var b=ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN);
        require(b.remaining()>=40 && b.getInt()==0x4d534554 && b.getInt()==1,"Expected little-endian MSET1");
        int tiles=b.getInt();b.position(32);int capacity=b.getInt(),polys=b.getInt();
        require(tiles>0 && tiles<=capacity && capacity>0 && capacity<=65536 && polys>0 && polys<=(1<<20),"Invalid mesh capacity");
        for(int i=0;i<tiles;i++) {
            require(b.remaining()>=16,"Truncated tile record");
            long ref=b.getLong();int size=b.getInt();b.getInt();
            require(ref!=0 && size>=100 && size<=32*1024*1024 && size<=b.remaining(),"Invalid tile length");
            int start=b.position();require(b.getInt(start)==0x444e4156 && b.getInt(start+4)==7,"Expected Detour7 tile");
            int pc=b.getInt(start+24),vc=b.getInt(start+28),lc=b.getInt(start+32),dc=b.getInt(start+36),
                dv=b.getInt(start+40),dt=b.getInt(start+44),bv=b.getInt(start+48),oc=b.getInt(start+52);
            require(pc>0 && pc<32768 && vc>=3 && vc<65535 && lc>0 && lc<=pc*24 && dc==pc &&
                dv>=0 && dv<=65535 && dt>=pc && dt<=pc*255 && bv>=0 && bv<=pc*2 && oc==0,"Invalid ground-mesh arrays");
            long expected=100L+vc*12L+pc*32L+lc*16L+dc*12L+dv*12L+dt*4L+bv*16L;
            require(expected==size,"Tile arrays do not match payload size");b.position(start+size);
        }
        require(!b.hasRemaining(),"Trailing mesh bytes");
        return new MeshSetReader().read(ByteBuffer.wrap(bytes),6);
    }
    static List<List<Double>> polygon(NavMesh mesh,long ref) {
        var pair=mesh.getTileAndPolyByRef(ref);require(pair.succeeded(),"Invalid corridor reference");
        var tile=pair.result.first;var poly=pair.result.second;
        require(poly.getType()==Poly.DT_POLYTYPE_GROUND,"Off-mesh links unsupported");
        var points=new ArrayList<List<Double>>();
        for(int i=0;i<poly.vertCount;i++) {
            int offset=poly.verts[i]*3;points.add(game(Arrays.copyOfRange(tile.data.verts,offset,offset+3)));
        }return points;
    }
    static float detailBoundaryHeight(NavMeshQuery q,long ref,float[] p) {
        var pair=q.getAttachedNavMesh().getTileAndPolyByRef(ref);
        require(pair.succeeded(),"Invalid surface polygon");
        var data=pair.result.first.data;var poly=pair.result.second;
        var detail=data.detailMeshes[poly.index];
        float distance=Float.POSITIVE_INFINITY,height=Float.NaN;
        for(int i=0;i<detail.triCount;i++) {
            float[][] vertices=new float[3][];
            for(int j=0;j<3;j++) {
                int index=data.detailTris[(detail.triBase+i)*4+j];
                boolean base=index<poly.vertCount;
                int offset=3*(base?poly.verts[index]:detail.vertBase+index-poly.vertCount);
                vertices[j]=Arrays.copyOfRange(base?data.verts:data.detailVerts,offset,offset+3);
            }
            for(int j=0;j<3;j++) {
                var a=vertices[j];var b=vertices[(j+1)%3];
                var edge=DetourCommon.distancePtSegSqr2D(p,a,b);
                if(edge.first<distance) {
                    distance=edge.first;height=a[1]+edge.second*(b[1]-a[1]);
                }
            }
        }
        require(distance<=0.1*0.1 && Float.isFinite(height),"No detail edge at polygon boundary sample");
        return height;
    }
    static float[] surface(NavMeshQuery q,long ref,float[] p) {
        var boundary=q.closestPointOnPolyBoundary(ref,p);
        require(boundary.succeeded() && near(p,boundary.result,0.1,20000000),"Sample leaves owning polygon");
        var point=boundary.result.clone();var height=q.getPolyHeight(ref,point);
        // recast4j 1.5.12 excludes some exact polygon edges in getPolyHeight.
        // closestPointOnPoly then searches only flagged detail boundary edges,
        // which can omit a portal. Check ownership against the base polygon,
        // then use its actual detail triangles without moving the route's XY.
        point[1]=height.succeeded()?height.result:detailBoundaryHeight(q,ref,point);
        require(Float.isFinite(point[1]),"No detail height at sample");return point;
    }
    static List<List<Double>> surfacePath(NavMeshQuery q,List<StraightPathItem> straight, List<Long> corridor) {
        var path=new ArrayList<List<Double>>();
        if(straight.size()==1) {path.add(game(surface(q,corridor.getFirst(),straight.getFirst().getPos())));return path;}
        for(int i=0;i+1<straight.size();i++) {
            var a=straight.get(i);var b=straight.get(i+1);long ref=a.getRef();
            require(ref!=0 && corridor.contains(ref),"Straight path lacks owning corridor polygon");
            double length=Math.hypot(a.getPos()[0]-b.getPos()[0],a.getPos()[2]-b.getPos()[2]);
            int steps=Math.max(1,(int)Math.ceil(length/8));
            require(path.size()+steps+1<=MAX_POINTS,"Surface path exceeds sample limit");
            // ALL_CROSSINGS splits at every portal. Each segment is inside its
            // owning convex polygon, so sampling cannot bridge two floors.
            for(int step=0;step<=steps;step++) {
                float t=(float)step/steps;float[] point=new float[3];
                for(int axis=0;axis<3;axis++) point[axis]=a.getPos()[axis]+t*(b.getPos()[axis]-a.getPos()[axis]);
                var v=game(surface(q,ref,point));
                if(path.isEmpty() || !path.getLast().equals(v)) path.add(v);
            }
        }return path;
    }
    static Map<String,Object> run(Path mesh, String request, String id, double[] from, double[] to,
                                  double horizontal, double vertical) throws Exception {
        return run(mesh,request,id,from,to,horizontal,vertical,5000,65535);
    }
    static Map<String,Object> run(Path mesh, String request, String id, double[] from, double[] to,
                                  double horizontal, double vertical,int timeoutMs,int maxNodes) throws Exception {
        return run(mesh,request,id,from,to,horizontal,vertical,timeoutMs,maxNodes,0,1,false);
    }
    @SuppressWarnings("unchecked")
    static Map<String,Object> run(Path mesh, String request, String id, double[] from, double[] to,
                                  double horizontal, double vertical,int timeoutMs,int maxNodes,
                                  int warmup,int measurements,boolean benchmark) throws Exception {
        require(timeoutMs>=1 && timeoutMs<=30000,"Search timeout must be 1..30000 ms");
        require(maxNodes>=1 && maxNodes<=1000000,"Max expanded nodes must be 1..1000000");
        require(warmup>=0 && warmup<=10000 && measurements>=1 && measurements<=1000,"Invalid benchmark counts");
        nav(from);nav(to);
        require(Double.isFinite(horizontal)&&horizontal>0&&horizontal<=32768&&Double.isFinite(vertical)&&vertical>0&&vertical<=32768,"Invalid snap limits");
        require(Files.size(mesh)<=512L*1024*1024,"Mesh exceeds size limit");
        long loadStart=System.nanoTime();byte[] bytes=Files.readAllBytes(mesh);NavMesh nav=read(bytes);
        long loadNs=System.nanoTime()-loadStart;
        var identity=new LinkedHashMap<String,Object>();identity.put("library","org.recast4j:detour:1.5.12");
        identity.put("mesh_sha256",HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(bytes)));
        identity.put("query","bounded sliced A*; ALL_CROSSINGS surface sampling");
        BenchmarkProgress.update("warmup",0,warmup);
        for(int i=0;i<warmup;i++) {
            benchmarkSink=query(nav,identity,request,id,from,to,horizontal,vertical,timeoutMs,maxNodes);
            BenchmarkProgress.update("warmup",i+1,warmup);
        }
        BenchmarkProgress.update("measurement",0,measurements);
        var samples=new ArrayList<Map<String,Object>>();
        Map<String,Object> result=null;
        for(int i=0;i<measurements;i++) {
            long started=System.nanoTime();
            result=query(nav,identity,request,id,from,to,horizontal,vertical,timeoutMs,maxNodes);
            long total=System.nanoTime()-started;
            var metrics=(Map<String,Object>)result.get("metrics");metrics.put("total_ns",total);
            if(benchmark) {
                var sample=new LinkedHashMap<String,Object>();sample.put("status",result.get("status"));
                for(String key:List.of("search_ns","snap_ns","smoothing_ns","validation_ns","total_ns","visited_nodes","limit_reason"))
                    sample.put(key,metrics.get(key));
                samples.add(sample);
            }
            benchmarkSink=result;
            BenchmarkProgress.update("measurement",i+1,measurements);
        }
        var metrics=(Map<String,Object>)result.get("metrics");metrics.put("load_ns",loadNs);
        if(benchmark) {
            metrics.put("benchmark_warmup",warmup);metrics.put("benchmark_samples",samples);
            metrics.put("mesh_load_count",1);
        }
        return result;
    }
    static Map<String,Object> query(NavMesh nav,Map<String,Object> identity,String request,String id,
                                   double[] from,double[] to,double horizontal,double vertical,int timeoutMs,int maxNodes) {
        var a=nav(from);var b=nav(to);
        var out=new LinkedHashMap<String,Object>();
        out.put("schema",1);out.put("request_id",request);out.put("case_id",id);out.put("backend","navmesh");
        out.put("status","failed");out.put("a",null);out.put("b",null);
        out.put("raw",List.of());out.put("final_path",List.of());out.put("validated_samples",List.of());
        out.put("corridor",List.of());out.put("segments_valid",null);out.put("identity",identity);out.put("diagnostic","");
        var metrics=new LinkedHashMap<String,Object>();out.put("metrics",metrics);
        metrics.put("load_ns",null);metrics.put("search_ns",null);metrics.put("smoothing_ns",null);metrics.put("validation_ns",null);
        metrics.put("visited_nodes",null);metrics.put("allocated_nodes",null);
        metrics.put("search_timeout_ms",timeoutMs);metrics.put("max_nodes",maxNodes);
        metrics.put("max_nodes_scope","expanded_nodes");metrics.put("limit_reason",null);
        metrics.put("baseline_server_budget_ms",100);
        var q=new NavMeshQuery(nav);var filter=new DefaultQueryFilter();long snapStart=System.nanoTime();
        float[] extents={(float)horizontal,(float)vertical,(float)horizontal};
        var start=q.findNearestPoly(a,extents,filter);var end=q.findNearestPoly(b,extents,filter);
        boolean av=start.succeeded() && start.result.getNearestRef()!=0 && near(a,start.result.getNearestPos(),horizontal,vertical);
        boolean bv=end.succeeded() && end.result.getNearestRef()!=0 && near(b,end.result.getNearestPos(),horizontal,vertical);
        if(start.succeeded() && start.result.getNearestRef()!=0) out.put("a",endpoint(from,start.result,av));
        if(end.succeeded() && end.result.getNearestRef()!=0) out.put("b",endpoint(to,end.result,bv));
        metrics.put("snap_ns",System.nanoTime()-snapStart);
        if(!av || !bv) {out.put("status","invalid_endpoint");out.put("diagnostic","No polygon within snap limits");return out;}
        var s=start.result;var e=end.result;long searchStart=System.nanoTime();
        Status status=q.initSlicedFindPath(s.getNearestRef(),e.getNearestRef(),s.getNearestPos(),e.getNearestPos(),filter,0);
        int iterations=0;
        long timeoutNs=timeoutMs*1000000L;
        while(status.isInProgress() && iterations<maxNodes && System.nanoTime()-searchStart<timeoutNs) {
            // Check the real deadline between node expansions; loading and
            // smoothing are deliberately outside the experimental search timer.
            var update=q.updateSlicedFindPath(1);status=update.status;
            if(update.result!=null) iterations+=update.result;
            require(update.result==null || update.result>0 || !status.isInProgress(),"Search made no progress");
        }
        long elapsed=System.nanoTime()-searchStart;
        metrics.put("allocated_nodes",q.getNodePool().getNodeMap().values().stream().mapToInt(List::size).sum());
        metrics.put("visited_nodes",iterations);
        if(elapsed>=timeoutNs || status.isInProgress()) {
            String reason=elapsed>=timeoutNs?"search_timeout":"node_limit";
            metrics.put("search_ns",elapsed);metrics.put("limit_reason",reason);
            out.put("status","resource_limit");
            out.put("diagnostic",reason.equals("search_timeout")?"Experimental search deadline reached ("+timeoutMs+" ms)":"Expanded-node limit reached ("+maxNodes+")");
            return out;
        }
        long finalizeStart=System.nanoTime();var found=q.finalizeSlicedFindPath();
        elapsed+=System.nanoTime()-finalizeStart;metrics.put("search_ns",elapsed);
        if(elapsed>=timeoutNs) {
            metrics.put("limit_reason","search_timeout");out.put("status","resource_limit");
            out.put("diagnostic","Experimental search deadline reached during path finalization ("+timeoutMs+" ms)");return out;
        }
        if(found.failed()) {out.put("status","failed");out.put("diagnostic","recast4j search failed: "+found.status);return out;}
        var corridor=found.result;
        if(corridor==null || corridor.isEmpty()) {out.put("status","no_path");return out;}
        boolean reached=!found.status.isPartial() && corridor.getLast()==e.getNearestRef();
        out.put("status",reached?"reached":"partial");
        var polygons=new ArrayList<List<List<Double>>>();for(long ref:corridor) polygons.add(polygon(nav,ref));out.put("corridor",polygons);
        float[] finish=e.getNearestPos();
        if(!reached) {
            var partial=q.closestPointOnPoly(corridor.getLast(),finish);require(partial.succeeded(),"Cannot resolve partial path endpoint");finish=partial.result.getClosest();
        }
        long smoothStart=System.nanoTime();
        var straight=q.findStraightPath(s.getNearestPos(),finish,corridor,LIMIT,NavMeshQuery.DT_STRAIGHTPATH_ALL_CROSSINGS);
        metrics.put("smoothing_ns",System.nanoTime()-smoothStart);
        if(straight.failed() || straight.result.size()>=LIMIT) {
            out.put("status","resource_limit");out.put("diagnostic","Straight path failed or reached output limit");return out;
        }
        long validationStart=System.nanoTime();
        try {
            var samples=surfacePath(q,straight.result,corridor);
            out.put("final_path",samples);out.put("validated_samples",samples);out.put("segments_valid",true);
        } catch(IllegalArgumentException invalid) {
            out.put("status","failed");out.put("segments_valid",false);out.put("diagnostic",invalid.getMessage());
        }
        metrics.put("validation_ns",System.nanoTime()-validationStart);return out;
    }
    static String json(Object value) {
        if(value==null) return "null";
        if(value instanceof String s) {
            var b=new StringBuilder("\"");for(char c:s.toCharArray()) {
                if(c=='"'||c=='\\') b.append('\\').append(c);
                else if(c<32) b.append(String.format(Locale.ROOT,"\\u%04x",(int)c));else b.append(c);
            }return b.append('"').toString();
        }
        if(value instanceof Number n) { require(Double.isFinite(n.doubleValue()),"Nonfinite JSON number");return n.toString(); }
        if(value instanceof Boolean b) return b.toString();
        if(value instanceof Map<?,?> m) {
            var items=new ArrayList<String>();m.forEach((k,v)->items.add(json(k.toString())+":"+json(v)));return "{"+String.join(",",items)+"}";
        }
        if(value instanceof List<?> l) return "["+String.join(",",l.stream().map(NavmeshRouteRunner::json).toList())+"]";
        throw new IllegalArgumentException("Unsupported JSON value");
    }
    static double[] xyz(String s) {
        var parts=s.split(",",-1);require(parts.length==3,"XYZ required");
        return new double[]{Double.parseDouble(parts[0]),Double.parseDouble(parts[1]),Double.parseDouble(parts[2])};
    }
    public static void main(String[] args) throws Exception {
        var options=new LinkedHashMap<String,String>();
        var required=Set.of("--mesh","--request-id","--case-id","--from","--to","--snap-horizontal","--snap-vertical","--output");
        var allowed=new HashSet<>(required);allowed.add("--search-timeout-ms");allowed.add("--max-nodes");
        allowed.add("--warmup");allowed.add("--iterations");
        require(args.length%2==0,"Expected named arguments");
        for(int i=0;i<args.length;i+=2) require(allowed.contains(args[i]) && options.putIfAbsent(args[i],args[i+1])==null,"Unknown or duplicate argument");
        require(options.keySet().containsAll(required),"Missing runner argument");
        var result=run(Path.of(options.get("--mesh")),options.get("--request-id"),options.get("--case-id"),
            xyz(options.get("--from")),xyz(options.get("--to")),Double.parseDouble(options.get("--snap-horizontal")),Double.parseDouble(options.get("--snap-vertical")),
            Integer.parseInt(options.getOrDefault("--search-timeout-ms","5000")),Integer.parseInt(options.getOrDefault("--max-nodes","65535")),
            Integer.parseInt(options.getOrDefault("--warmup","0")),Integer.parseInt(options.getOrDefault("--iterations","1")),
            options.containsKey("--warmup") || options.containsKey("--iterations"));
        Path output=Path.of(options.get("--output")).toAbsolutePath();
        byte[] encoded=(json(result)+"\n").getBytes(java.nio.charset.StandardCharsets.UTF_8);
        require(encoded.length<=16*1024*1024,"Result exceeds 16 MiB protocol limit");
        Path temporary=Files.createTempFile(output.getParent(),".route-",".tmp");
        try {
            Files.write(temporary,encoded);
            // Exclusive atomic publication on local NTFS: never replace an old report.
            Files.createLink(output,temporary);
        } finally { Files.deleteIfExists(temporary); }
    }
}
