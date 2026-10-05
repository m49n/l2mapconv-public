#include <DetourNavMeshQuery.h>
#include <cmath>
#include <navmesh/Navmesh.h>
#include <stdexcept>
#include <vector>
namespace navmesh {
bool reachable(const dtNavMesh &mesh, glm::vec3 from, glm::vec3 to,
               float horizontal, float vertical) {
  for (float v :
       {from.x, from.y, from.z, to.x, to.y, to.z, horizontal, vertical})
    if (!std::isfinite(v))
      throw std::invalid_argument("Nonfinite navmesh query");
  if (horizontal < 0 || vertical < 0)
    throw std::invalid_argument("Negative query tolerance");
  dtNavMeshQuery query;
  if (dtStatusFailed(query.init(&mesh, 65535)))
    throw std::runtime_error("Cannot initialize navmesh query");
  const float a[]{from.x, from.z, from.y}, b[]{to.x, to.z, to.y},
      extents[]{horizontal, vertical, horizontal};
  float pa[3]{}, pb[3]{};
  dtPolyRef ra = 0, rb = 0;
  dtQueryFilter filter;
  if (dtStatusFailed(query.findNearestPoly(a, extents, &filter, &ra, pa)) ||
      dtStatusFailed(query.findNearestPoly(b, extents, &filter, &rb, pb)) ||
      !ra || !rb)
    return false;
  // findNearestPoly searches polygon bounds: also bound the actual snap
  // distance.
  const auto near = [&](const float *p, const float *q) {
    return std::hypot(p[0] - q[0], p[2] - q[2]) <= horizontal &&
           std::abs(p[1] - q[1]) <= vertical;
  };
  if (!near(a, pa) || !near(b, pb))
    return false;
  std::vector<dtPolyRef> path(65535);
  int count = 0;
  const auto status = query.findPath(ra, rb, pa, pb, &filter, path.data(),
                                     &count, static_cast<int>(path.size()));
  if (dtStatusDetail(status, DT_OUT_OF_NODES) ||
      dtStatusDetail(status, DT_BUFFER_TOO_SMALL))
    throw std::runtime_error("Navmesh query resource limit exceeded");
  return dtStatusSucceed(status) && count > 0 && path[count - 1] == rb;
}
} // namespace navmesh
