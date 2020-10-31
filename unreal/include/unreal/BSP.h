#pragma once

#include "Archive.h"
#include "Index.h"
#include "Material.h"
#include "Object.h"
#include "ObjectRef.h"
#include "Primitives.h"

#include <cstdint>

namespace unreal {

class Primitive : public Object {
public:
  Box bounding_box;
  Sphere bounding_sphere;

  explicit Primitive(Archive &archive) : Object{archive} {}

  virtual void deserialize() override;
};

enum BSPNodeFlags {
  // Flags.
  NF_NotCsg = 0x01, // Node is not a Csg splitter, i.e. is a transparent poly.
  NF_ShootThrough = 0x02,   // Can shoot through (for projectile solid ops).
  NF_NotVisBlocking = 0x04, // Node does not block visibility, i.e. is an
  // invisible collision hull.
  NF_PolyOccluded =
      0x08, // Node's poly was occluded on the previously-drawn frame.
  NF_BoxOccluded = 0x10,   // Node's bounding box was occluded.
  NF_BrightCorners = 0x10, // Temporary.
  NF_IsNew = 0x20,         // Editor: Node was newly-added.
  NF_IsFront = 0x40,       // Filter operation bounding-sphere precomputed and
  // guaranteed to be front.
  NF_IsBack = 0x80, // Guaranteed back.

  // Combinations of flags.
  NF_NeverMove = 0x00, // Bsp cleanup must not move nodes with these tags.

  NF_Passable = NF_NotCsg,
};

struct BSPNode {
  Plane plane; // Plane the node falls into (X, Y, Z, W).
  std::uint64_t
      zone_mask; // Bit mask for all zones at or below this node (up to 64).
  std::uint8_t flags;      // Node flags.
  Index vertex_pool_index; // Index of first vertex in vertex pool, =terrain if
                           // num_vertices==0 and NF_TerrainFront.
  Index surface_index;     // Index to surface information.
  Index back_index;        // Index to node in front (in direction of Normal).
  Index front_index; // Index to node in back  (opposite direction as Normal).
  Index plane_index; // Index to next coplanar poly in coplanar list.
  Index collision_bound;
  Index render_bound;
  Vector unknown_point;
  std::int32_t unknown_id;
  std::uint64_t
      connectivity; // (Connect[i]&(1<<j))==1 if zone i is adjacent to zone j.
  std::uint64_t visibility;  // (Connect[i]&(1<<j))==1 if zone i can see zone j.
  Index zone[2];             // Visibility zone in 1=front, 0=back.
  std::uint8_t vertex_count; // Number of vertices in node.
  std::int32_t leaf[2];      // Leaf in back and front, INDEX_NONE=not a leaf.

  friend auto operator>>(Archive &archive, BSPNode &node) -> Archive &;
};

enum BSPPolygonFlags {
  // Regular in-game flags.
  PF_Invisible = 0x00000001,     // Poly is invisible.
  PF_Masked = 0x00000002,        // Poly should be drawn masked.
  PF_Translucent = 0x00000004,   // Poly is transparent.
  PF_NotSolid = 0x00000008,      // Poly is not solid, doesn't block.
  PF_Environment = 0x00000010,   // Poly should be drawn environment mapped.
  PF_ForceViewZone = 0x00000010, // Force current iViewZone in OccludeBSP (reuse
  // Environment flag)
  PF_Semisolid =
      0x00000020, // Poly is semi-solid = collision solid, Csg nonsolid.
  PF_Modulated = 0x00000040,    // Modulation transparency.
  PF_FakeBackdrop = 0x00000080, // Poly looks exactly like backdrop.
  PF_TwoSided = 0x00000100,     // Poly is visible from both sides.
  PF_AutoUPan = 0x00000200,     // Automatically pans in U direction.
  PF_AutoVPan = 0x00000400,     // Automatically pans in V direction.
  PF_NoSmooth = 0x00000800,     // Don't smooth textures.
  PF_BigWavy = 0x00001000,      // Poly has a big wavy pattern in it.
  PF_SpecialPoly = 0x00001000, // Game-specific poly-level render control (reuse
  // BigWavy flag)
  PF_SmallWavy =
      0x00002000,       // Small wavy pattern (for water/enviro reflection).
  PF_Flat = 0x00004000, // Flat surface.
  PF_LowShadowDetail = 0x00008000, // Low detaul shadows.
  PF_NoMerge =
      0x00010000, // Don't merge poly's nodes before lighting when rendering.
  PF_CloudWavy = 0x00020000,     // Polygon appears wavy like clouds.
  PF_DirtyShadows = 0x00040000,  // Dirty shadows.
  PF_BrightCorners = 0x00080000, // Brighten convex corners.
  PF_SpecialLit = 0x00100000,    // Only speciallit lights apply to this poly.
  PF_Gouraud = 0x00200000,       // Gouraud shaded.
  PF_NoBoundRejection =
      0x00200000, // Disable bound rejection in OccludeBSP (reuse Gourard flag)
  PF_Unlit = 0x00400000,            // Unlit.
  PF_HighShadowDetail = 0x00800000, // High detail shadows.
  PF_Portal = 0x04000000,           // Portal between iZones.
  PF_Mirrored = 0x08000000,         // Reflective surface.

  // Editor flags.
  PF_Memorized = 0x01000000,   // Editor: Poly is remembered.
  PF_Selected = 0x02000000,    // Editor: Poly is selected.
  PF_Highlighted = 0x10000000, // Editor: Poly is highlighted.
  PF_FlatShaded = 0x40000000,  // FPoly has been split by SplitPolyWithPlane.

  // Internal.
  PF_EdProcessed =
      0x40000000,        // FPoly was already processed in editorBuildFPolys.
  PF_EdCut = 0x80000000, // FPoly has been split by SplitPolyWithPlane.
  PF_RenderFog = 0x40000000,  // Render with fogmapping.
  PF_Occlude = 0x80000000,    // Occludes even if PF_NoOcclude.
  PF_RenderHint = 0x01000000, // Rendering optimization hint.

  // Combinations of flags.
  PF_NoOcclude = PF_Masked | PF_Translucent | PF_Invisible | PF_Modulated,
  PF_NoEdit =
      PF_Memorized | PF_Selected | PF_EdProcessed | PF_NoMerge | PF_EdCut,
  PF_NoImport = PF_NoEdit | PF_NoMerge | PF_Memorized | PF_Selected |
                PF_EdProcessed | PF_EdCut,
  PF_AddLast = PF_Semisolid | PF_NotSolid,
  PF_NoAddToBSP = PF_EdCut | PF_EdProcessed | PF_Selected | PF_Memorized,
  PF_NoShadows = PF_Unlit | PF_Invisible | PF_Environment | PF_FakeBackdrop,
  PF_Transient = PF_Highlighted,

  PF_Passable =
      PF_NoOcclude | PF_NoImport | PF_NotSolid | PF_NoShadows | PF_Portal,
};

struct BSPSurface {
  ObjectRef<Material> material;
  std::uint32_t polygon_flags;
  Index base_index;   // Polygon & texture base point index (where U,V==0,0).
  Index normal_index; // Index to polygon normal.
  Index u_index;      // Texture U-vector index.
  Index v_index;      // Texture V-vector index.
  ObjectRef<Object> brush_polygon; // Editor brush polygon.
  ObjectRef<Object> actor;         // Brush actor owning this BSP surface.
  Plane plane;
  std::uint32_t nodes[2]; // Nodes which make up this surface.

  friend auto operator>>(Archive &archive, BSPSurface &surface) -> Archive &;
};

struct Vertex {
  Index vertex_index; // Index of vertex.
  Index side_index;   // If shared, index of unique side. Otherwise INDEX_NONE.

  friend auto operator>>(Archive &archive, Vertex &vertex) -> Archive &;
};

class Model : public Primitive {
public:
  std::vector<Vector> vectors;
  std::vector<Vector> points;
  std::vector<BSPNode> nodes;
  std::vector<BSPSurface> surfaces;
  std::vector<Vertex> vertices;

  explicit Model(Archive &archive) : Primitive{archive} {}

  virtual void deserialize() override;
};

} // namespace unreal
