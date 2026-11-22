#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

struct Vec3f {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

/**
 * Polygon mesh: faces can be triangles, quads or n-gons (needed for real modeling).
 * M1 stores positions + faces only. Adjacency (half-edge) is added in M2 for editing.
 */
class Mesh {
public:
    std::vector<Vec3f> positions;
    std::vector<uint32_t> faceStart{0};  // faceCount() + 1 entries
    std::vector<uint32_t> faceVerts;     // all faces' vertex indices, back to back

    size_t vertexCount() const { return positions.size(); }
    size_t faceCount() const { return faceStart.size() - 1; }
    size_t triangleCount() const;

    void clear();
    void addFace(const uint32_t* v, size_t n);
    bool bounds(Vec3f& mn, Vec3f& mx) const;

    /** Merge vertices closer than eps; drops faces that collapse. */
    void weld(float eps);

    /** Unit cube made of 6 quads, outward CCW winding. */
    static Mesh makeCube();
};
