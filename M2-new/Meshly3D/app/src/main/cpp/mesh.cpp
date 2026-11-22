#include "mesh.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

size_t Mesh::triangleCount() const {
    size_t t = 0;
    for (size_t f = 0; f < faceCount(); ++f) {
        const size_t n = faceStart[f + 1] - faceStart[f];
        if (n >= 3) t += n - 2;
    }
    return t;
}

void Mesh::clear() {
    positions.clear();
    faceStart.assign(1, 0);
    faceVerts.clear();
}

void Mesh::addFace(const uint32_t* v, size_t n) {
    faceVerts.insert(faceVerts.end(), v, v + n);
    faceStart.push_back(static_cast<uint32_t>(faceVerts.size()));
}

bool Mesh::bounds(Vec3f& mn, Vec3f& mx) const {
    if (positions.empty()) return false;
    mn = mx = positions[0];
    for (const Vec3f& p : positions) {
        mn.x = std::min(mn.x, p.x);
        mn.y = std::min(mn.y, p.y);
        mn.z = std::min(mn.z, p.z);
        mx.x = std::max(mx.x, p.x);
        mx.y = std::max(mx.y, p.y);
        mx.z = std::max(mx.z, p.z);
    }
    return true;
}

namespace {
struct CellKey {
    int64_t x, y, z;
    bool operator==(const CellKey& o) const { return x == o.x && y == o.y && z == o.z; }
};
struct CellHash {
    size_t operator()(const CellKey& k) const {
        uint64_t h = static_cast<uint64_t>(k.x) * 73856093ULL ^ static_cast<uint64_t>(k.y) * 19349663ULL ^
                     static_cast<uint64_t>(k.z) * 83492791ULL;
        return static_cast<size_t>(h);
    }
};
}  // namespace

void Mesh::weld(float epsF) {
    if (positions.empty() || !(epsF > 0.0f)) return;
    const double eps = epsF;
    const double cell = 2.0 * eps;  // a point's eps-neighbourhood touches at most 2 cells per axis

    std::unordered_map<CellKey, std::vector<uint32_t>, CellHash> grid;
    grid.reserve(positions.size() * 2);
    std::vector<Vec3f> merged;
    merged.reserve(positions.size());
    std::vector<uint32_t> remap(positions.size());

    for (size_t i = 0; i < positions.size(); ++i) {
        const Vec3f p = positions[i];
        const double c[3] = {p.x, p.y, p.z};
        int64_t lo[3], hi[3];
        for (int a = 0; a < 3; ++a) {
            lo[a] = static_cast<int64_t>(std::floor((c[a] - eps) / cell));
            hi[a] = static_cast<int64_t>(std::floor((c[a] + eps) / cell));
        }
        uint32_t found = UINT32_MAX;
        for (int64_t x = lo[0]; x <= hi[0] && found == UINT32_MAX; ++x)
            for (int64_t y = lo[1]; y <= hi[1] && found == UINT32_MAX; ++y)
                for (int64_t z = lo[2]; z <= hi[2] && found == UINT32_MAX; ++z) {
                    auto it = grid.find(CellKey{x, y, z});
                    if (it == grid.end()) continue;
                    for (uint32_t idx : it->second) {
                        const Vec3f& q = merged[idx];
                        const double dx = q.x - c[0], dy = q.y - c[1], dz = q.z - c[2];
                        if (dx * dx + dy * dy + dz * dz <= eps * eps) {
                            found = idx;
                            break;
                        }
                    }
                }
        if (found == UINT32_MAX) {
            found = static_cast<uint32_t>(merged.size());
            merged.push_back(p);
            const CellKey own{static_cast<int64_t>(std::floor(c[0] / cell)),
                              static_cast<int64_t>(std::floor(c[1] / cell)),
                              static_cast<int64_t>(std::floor(c[2] / cell))};
            grid[own].push_back(found);
        }
        remap[i] = found;
    }

    std::vector<uint32_t> newStart{0};
    std::vector<uint32_t> newVerts;
    newVerts.reserve(faceVerts.size());
    for (size_t f = 0; f < faceCount(); ++f) {
        const uint32_t s = faceStart[f], e = faceStart[f + 1];
        const size_t n = e - s;
        bool degenerate = n < 3;
        for (size_t a = 0; a < n && !degenerate; ++a)
            for (size_t b = a + 1; b < n; ++b)
                if (remap[faceVerts[s + a]] == remap[faceVerts[s + b]]) {
                    degenerate = true;
                    break;
                }
        if (degenerate) continue;
        for (uint32_t k = s; k < e; ++k) newVerts.push_back(remap[faceVerts[k]]);
        newStart.push_back(static_cast<uint32_t>(newVerts.size()));
    }
    positions.swap(merged);
    faceStart.swap(newStart);
    faceVerts.swap(newVerts);
}

Mesh Mesh::makeCube() {
    Mesh m;
    const float h = 0.5f;
    const float c[8][3] = {{-h, -h, -h}, {h, -h, -h}, {h, h, -h}, {-h, h, -h},
                           {-h, -h, h},  {h, -h, h},  {h, h, h},  {-h, h, h}};
    for (const auto& p : c) m.positions.push_back({p[0], p[1], p[2]});
    const uint32_t faces[6][4] = {{4, 5, 6, 7},   // +Z
                                  {1, 0, 3, 2},   // -Z
                                  {5, 1, 2, 6},   // +X
                                  {0, 4, 7, 3},   // -X
                                  {7, 6, 2, 3},   // +Y
                                  {0, 1, 5, 4}};  // -Y
    for (const auto& f : faces) m.addFace(f, 4);
    return m;
}
