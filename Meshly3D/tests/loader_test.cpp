// Host-side tests for Mesh + GLB loader. Build/run: see .github/workflows/android.yml
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "glb_loader.h"
#include "mesh.h"

static int g_fail = 0;
#define CHECK(cond)                                                  \
    do {                                                             \
        if (!(cond)) {                                               \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
            ++g_fail;                                                \
        }                                                            \
    } while (0)

static bool readFile(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
}

static double signedVolume(const Mesh& m) {
    double v = 0;
    for (size_t f = 0; f < m.faceCount(); ++f) {
        const uint32_t s = m.faceStart[f], e = m.faceStart[f + 1];
        for (uint32_t k = s + 1; k + 1 < e; ++k) {
            const Vec3f a = m.positions[m.faceVerts[s]], b = m.positions[m.faceVerts[k]],
                        c = m.positions[m.faceVerts[k + 1]];
            const double cx = double(b.y) * c.z - double(b.z) * c.y;
            const double cy = double(b.z) * c.x - double(b.x) * c.z;
            const double cz = double(b.x) * c.y - double(b.y) * c.x;
            v += (a.x * cx + a.y * cy + a.z * cz) / 6.0;
        }
    }
    return v;
}

int main(int argc, char** argv) {
    const std::string dir = argc > 1 ? argv[1] : "samples";

    // 1. built-in cube: 8 vertices, 6 quads, volume 1, outward winding
    {
        Mesh c = Mesh::makeCube();
        CHECK(c.vertexCount() == 8);
        CHECK(c.faceCount() == 6);
        CHECK(c.triangleCount() == 12);
        CHECK(std::fabs(signedVolume(c) - 1.0) < 1e-6);
    }

    // 2. torus: split seam vertices must be welded, quaternion + translation applied
    std::vector<uint8_t> torus;
    CHECK(readFile(dir + "/torus.glb", torus));
    {
        Mesh m;
        const std::string err = loadGlb(torus.data(), torus.size(), 700000, m);
        if (!err.empty()) std::printf("torus error: %s\n", err.c_str());
        CHECK(err.empty());
        CHECK(m.vertexCount() == 288);  // 24 x 12 after welding the duplicated seam
        CHECK(m.faceCount() == 576);
        const double vol = signedVolume(m);
        CHECK(vol > 1.4 && vol < 1.9);  // exact torus is 1.7765, polygonal a bit less
        Vec3f mn, mx;
        CHECK(m.bounds(mn, mx));
        CHECK(std::fabs(mx.y - 1.8f) < 0.01f);   // rotated 90 deg about X, moved +0.5 in Y
        CHECK(std::fabs(mn.y + 0.8f) < 0.01f);
        CHECK(std::fabs(mx.z - 0.3f) < 0.01f);
    }

    // 3. two cubes: matrix node, TRS node, uint32 indices, mirrored node (winding flip)
    {
        std::vector<uint8_t> d;
        CHECK(readFile(dir + "/two_cubes.glb", d));
        Mesh m;
        const std::string err = loadGlb(d.data(), d.size(), 700000, m);
        if (!err.empty()) std::printf("two_cubes error: %s\n", err.c_str());
        CHECK(err.empty());
        CHECK(m.vertexCount() == 16);
        CHECK(m.faceCount() == 24);
        CHECK(std::fabs(signedVolume(m) - 9.0) < 1e-3);  // 8 (scaled cube) + 1 (mirrored cube)
    }

    // 4. triangle limit
    {
        Mesh m;
        CHECK(!loadGlb(torus.data(), torus.size(), 100, m).empty());
        CHECK(m.faceCount() == 0);
    }

    // 5. garbage / truncated input must give an error, never crash
    {
        Mesh m;
        CHECK(!loadGlb(nullptr, 0, 1000, m).empty());
        const uint8_t junk[16] = {1, 2, 3};
        CHECK(!loadGlb(junk, sizeof(junk), 1000, m).empty());
        for (size_t cut : {size_t(12), size_t(20), size_t(100), torus.size() / 2, torus.size() - 1})
            CHECK(!loadGlb(torus.data(), cut, 700000, m).empty());
    }

    // 6. fuzz: random byte damage (run with sanitizers; result does not matter, crashing does)
    {
        unsigned long long s = 88172645463325252ULL;
        auto rnd = [&]() {
            s ^= s << 13;
            s ^= s >> 7;
            s ^= s << 17;
            return s;
        };
        int ok = 0;
        for (int it = 0; it < 400; ++it) {
            std::vector<uint8_t> d = torus;
            const int hits = 1 + int(rnd() % 8);
            for (int h = 0; h < hits; ++h) d[rnd() % d.size()] = uint8_t(rnd());
            if (it % 5 == 0) d.resize(size_t(rnd() % d.size()) + 1);
            Mesh m;
            if (loadGlb(d.data(), d.size(), 700000, m).empty()) ++ok;
        }
        std::printf("fuzz: %d of 400 damaged files still loaded (fine)\n", ok);
    }

    if (g_fail) {
        std::printf("%d CHECK(S) FAILED\n", g_fail);
        return 1;
    }
    std::printf("ALL TESTS PASSED\n");
    return 0;
}
