#pragma once
// Editing core of Meshly3D (M2): adjacency tables, selection (vertex / edge / face), picking, box / lasso,
// gizmo drag math (move / rotate / scale on an axis) and undo / redo of vertex moves.
// Header-only on purpose: it is unit-tested on the host without Android (tests/edit_tests.h).
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <limits>
#include <utility>
#include <vector>

#include "mathutil.h"
#include "mesh.h"

namespace edit {

using namespace m3;

struct Vec2 {
    float x = 0.0f, y = 0.0f;
};

// ------------------------------------------------------------------------------------------------ camera
/** Screen <-> world mapping. Screen coordinates: pixels, origin top-left, y down (like Android touches). */
struct Camera {
    double vp[16] = {0};
    double inv[16] = {0};
    int width = 1, height = 1;
    Vec3f eye;
    bool valid = false;

    void setup(const float* viewProj, int w, int h, Vec3f eyePos) {
        for (int i = 0; i < 16; ++i) vp[i] = viewProj[i];
        width = std::max(w, 1);
        height = std::max(h, 1);
        eye = eyePos;
        valid = invert4(vp, inv);
    }

    bool project(Vec3f p, Vec2& s, float* depth = nullptr) const {
        const double x = p.x, y = p.y, z = p.z;
        const double cx = vp[0] * x + vp[4] * y + vp[8] * z + vp[12];
        const double cy = vp[1] * x + vp[5] * y + vp[9] * z + vp[13];
        const double cz = vp[2] * x + vp[6] * y + vp[10] * z + vp[14];
        const double cw = vp[3] * x + vp[7] * y + vp[11] * z + vp[15];
        if (cw <= 1e-9) return false;  // behind the camera
        s.x = static_cast<float>((cx / cw * 0.5 + 0.5) * width);
        s.y = static_cast<float>((1.0 - (cy / cw * 0.5 + 0.5)) * height);
        if (depth) *depth = static_cast<float>(cz / cw);
        return true;
    }

    /** World-space ray through a pixel. */
    bool ray(float sx, float sy, Vec3f& origin, Vec3f& dir) const {
        if (!valid) return false;
        const double nx = sx / width * 2.0 - 1.0;
        const double ny = 1.0 - sy / height * 2.0;
        double a[3], b[3];
        if (!unproject(nx, ny, -1.0, a) || !unproject(nx, ny, 1.0, b)) return false;
        const double dx = b[0] - a[0], dy = b[1] - a[1], dz = b[2] - a[2];
        const double l = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (!(l > 0.0)) return false;
        origin = {static_cast<float>(a[0]), static_cast<float>(a[1]), static_cast<float>(a[2])};
        dir = {static_cast<float>(dx / l), static_cast<float>(dy / l), static_cast<float>(dz / l)};
        return true;
    }

private:
    bool unproject(double nx, double ny, double nz, double* out) const {
        const double x = inv[0] * nx + inv[4] * ny + inv[8] * nz + inv[12];
        const double y = inv[1] * nx + inv[5] * ny + inv[9] * nz + inv[13];
        const double z = inv[2] * nx + inv[6] * ny + inv[10] * nz + inv[14];
        const double w = inv[3] * nx + inv[7] * ny + inv[11] * nz + inv[15];
        if (std::fabs(w) < 1e-12) return false;
        out[0] = x / w;
        out[1] = y / w;
        out[2] = z / w;
        return true;
    }
};

// ------------------------------------------------------------------------------------------------ topology
/** Adjacency tables: unique edges, edge of every face corner, faces around edges and vertices. */
struct Topology {
    std::vector<uint32_t> edgeA, edgeB;               // endpoints, edgeA < edgeB
    std::vector<uint32_t> faceEdges;                  // per face corner k: edge from corner k to the next corner
    std::vector<uint32_t> edgeFaceStart, edgeFaces;   // CSR: faces using each edge
    std::vector<uint32_t> vertFaceStart, vertFaces;   // CSR: faces around each vertex

    size_t edgeCount() const { return edgeA.size(); }

    void build(const Mesh& m) {
        edgeA.clear();
        edgeB.clear();
        faceEdges.clear();
        edgeFaceStart.clear();
        edgeFaces.clear();
        const size_t nc = m.faceVerts.size();
        const size_t nf = m.faceCount();
        const size_t nv = m.vertexCount();

        std::vector<uint32_t> cornerFace(nc);
        struct Item {
            uint64_t key;
            uint32_t corner;
        };
        std::vector<Item> items(nc);
        for (size_t f = 0; f < nf; ++f) {
            const uint32_t s = m.faceStart[f], e = m.faceStart[f + 1];
            for (uint32_t k = s; k < e; ++k) {
                uint32_t a = m.faceVerts[k], b = m.faceVerts[k + 1 < e ? k + 1 : s];
                if (a > b) std::swap(a, b);
                items[k] = {(static_cast<uint64_t>(a) << 32) | b, k};
                cornerFace[k] = static_cast<uint32_t>(f);
            }
        }
        std::sort(items.begin(), items.end(), [](const Item& x, const Item& y) {
            return x.key != y.key ? x.key < y.key : x.corner < y.corner;
        });
        faceEdges.assign(nc, 0);
        for (size_t i = 0; i < nc;) {
            const uint32_t eid = static_cast<uint32_t>(edgeA.size());
            edgeA.push_back(static_cast<uint32_t>(items[i].key >> 32));
            edgeB.push_back(static_cast<uint32_t>(items[i].key & 0xFFFFFFFFu));
            edgeFaceStart.push_back(static_cast<uint32_t>(edgeFaces.size()));
            size_t j = i;
            while (j < nc && items[j].key == items[i].key) {
                faceEdges[items[j].corner] = eid;
                edgeFaces.push_back(cornerFace[items[j].corner]);
                ++j;
            }
            i = j;
        }
        edgeFaceStart.push_back(static_cast<uint32_t>(edgeFaces.size()));

        vertFaceStart.assign(nv + 1, 0);
        for (uint32_t v : m.faceVerts)
            if (v < nv) vertFaceStart[v + 1]++;
        for (size_t i = 0; i < nv; ++i) vertFaceStart[i + 1] += vertFaceStart[i];
        vertFaces.assign(vertFaceStart[nv], 0);
        std::vector<uint32_t> cursor(vertFaceStart.begin(), vertFaceStart.end() - 1);
        for (size_t f = 0; f < nf; ++f)
            for (uint32_t k = m.faceStart[f]; k < m.faceStart[f + 1]; ++k) {
                const uint32_t v = m.faceVerts[k];
                if (v < nv) vertFaces[cursor[v]++] = static_cast<uint32_t>(f);
            }
    }
};

// ------------------------------------------------------------------------------------------------ session
class EditSession {
public:
    enum Elem { Vertex = 0, Edge = 1, Face = 2 };
    enum Tool { Move = 0, Rotate = 1, Scale = 2 };

    struct Gizmo {
        bool valid = false;
        Vec3f pivot;
        Vec3f tip[3];  // end of the X, Y, Z handles
        float len = 0.0f;
    };

    static constexpr size_t kMaxUndoEntries = 200;
    static constexpr size_t kMaxUndoBytes = 96u * 1024u * 1024u;

    // ---- setup
    void attach(Mesh* mesh) {
        mesh_ = mesh;
        topo_.build(*mesh_);
        sel_[0].assign(mesh_->vertexCount(), 0);
        sel_[1].assign(topo_.edgeCount(), 0);
        sel_[2].assign(mesh_->faceCount(), 0);
        undo_.clear();
        redo_.clear();
        undoBytes_ = 0;
        dragging_ = false;
        cacheDirty_ = true;
        ++selVersion_;
        ++geoVersion_;
    }

    const Topology& topology() const { return topo_; }
    uint64_t selVersion() const { return selVersion_; }
    uint64_t geoVersion() const { return geoVersion_; }
    Elem elem() const { return mode_; }

    // ---- selection
    void setElem(Elem e) {
        if (e == mode_) return;
        if (!mesh_) {
            mode_ = e;
            return;
        }
        const Mesh& m = *mesh_;
        std::vector<uint8_t> out(sel_[e].size(), 0);
        if (mode_ == Vertex) {
            if (e == Edge) {
                for (size_t i = 0; i < topo_.edgeCount(); ++i)
                    out[i] = (sel_[0][topo_.edgeA[i]] && sel_[0][topo_.edgeB[i]]) ? 1 : 0;
            } else {
                for (size_t f = 0; f < m.faceCount(); ++f) {
                    bool all = m.faceStart[f + 1] > m.faceStart[f];
                    for (uint32_t k = m.faceStart[f]; k < m.faceStart[f + 1] && all; ++k)
                        all = sel_[0][m.faceVerts[k]] != 0;
                    out[f] = all ? 1 : 0;
                }
            }
        } else if (mode_ == Edge) {
            if (e == Vertex) {
                for (size_t i = 0; i < topo_.edgeCount(); ++i)
                    if (sel_[1][i]) out[topo_.edgeA[i]] = out[topo_.edgeB[i]] = 1;
            } else {
                for (size_t f = 0; f < m.faceCount(); ++f) {
                    bool all = m.faceStart[f + 1] > m.faceStart[f];
                    for (uint32_t k = m.faceStart[f]; k < m.faceStart[f + 1] && all; ++k)
                        all = sel_[1][topo_.faceEdges[k]] != 0;
                    out[f] = all ? 1 : 0;
                }
            }
        } else {  // Face
            for (size_t f = 0; f < m.faceCount(); ++f) {
                if (!sel_[2][f]) continue;
                for (uint32_t k = m.faceStart[f]; k < m.faceStart[f + 1]; ++k) {
                    if (e == Vertex) out[m.faceVerts[k]] = 1;
                    else out[topo_.faceEdges[k]] = 1;
                }
            }
        }
        std::fill(sel_[mode_].begin(), sel_[mode_].end(), 0);
        sel_[e].swap(out);
        mode_ = e;
        ++selVersion_;
    }

    size_t selectedCount() const {
        size_t n = 0;
        for (uint8_t s : sel_[mode_]) n += s ? 1 : 0;
        return n;
    }
    bool hasSelection() const {
        for (uint8_t s : sel_[mode_])
            if (s) return true;
        return false;
    }
    bool isSelected(Elem kind, size_t i) const { return i < sel_[kind].size() && sel_[kind][i] != 0; }

    void clearSelection() {
        std::fill(sel_[mode_].begin(), sel_[mode_].end(), 0);
        ++selVersion_;
    }
    void selectAll() {
        std::fill(sel_[mode_].begin(), sel_[mode_].end(), 1);
        ++selVersion_;
    }

    /** Taps / clicks. Returns true if an element was hit. */
    bool tapSelect(const Camera& cam, float x, float y, float radiusPx, bool extend) {
        if (!mesh_) return false;
        refreshCache();
        int hit = -1;
        if (mode_ == Vertex) hit = pickVertex(cam, x, y, radiusPx);
        else if (mode_ == Edge) hit = pickEdge(cam, x, y, radiusPx);
        else hit = pickFace(cam, x, y);
        if (hit < 0) {
            if (!extend) clearSelection();
            return false;
        }
        std::vector<uint8_t>& arr = sel_[mode_];
        if (extend) {
            arr[hit] = arr[hit] ? 0 : 1;
        } else {
            std::fill(arr.begin(), arr.end(), 0);
            arr[hit] = 1;
        }
        ++selVersion_;
        return true;
    }

    /** Box / lasso: every visible element of the current kind inside the screen polygon. */
    void regionSelect(const Camera& cam, const std::vector<Vec2>& poly, bool extend) {
        if (!mesh_ || poly.size() < 3) return;
        refreshCache();
        std::vector<uint8_t>& arr = sel_[mode_];
        if (!extend) std::fill(arr.begin(), arr.end(), 0);
        const Mesh& m = *mesh_;
        if (mode_ == Vertex) {
            for (size_t v = 0; v < m.vertexCount(); ++v) {
                Vec2 s;
                if (cam.project(m.positions[v], s) && insidePoly(poly, s.x, s.y) && vertexVisible(v, cam.eye)) arr[v] = 1;
            }
        } else if (mode_ == Edge) {
            for (size_t e = 0; e < topo_.edgeCount(); ++e) {
                Vec2 a, b;
                if (cam.project(m.positions[topo_.edgeA[e]], a) && cam.project(m.positions[topo_.edgeB[e]], b) &&
                    insidePoly(poly, a.x, a.y) && insidePoly(poly, b.x, b.y) && edgeVisible(e, cam.eye))
                    arr[e] = 1;
            }
        } else {
            for (size_t f = 0; f < m.faceCount(); ++f) {
                Vec2 s;
                if (frontFacing(f, cam.eye) && cam.project(fC_[f], s) && insidePoly(poly, s.x, s.y)) arr[f] = 1;
            }
        }
        ++selVersion_;
    }

    /** Vertices that a transform would move (sorted, unique). */
    std::vector<uint32_t> affectedVertices() const {
        std::vector<uint32_t> out;
        if (!mesh_) return out;
        const Mesh& m = *mesh_;
        std::vector<uint8_t> mask(m.vertexCount(), 0);
        if (mode_ == Vertex) {
            mask = sel_[0];
        } else if (mode_ == Edge) {
            for (size_t e = 0; e < topo_.edgeCount(); ++e)
                if (sel_[1][e]) mask[topo_.edgeA[e]] = mask[topo_.edgeB[e]] = 1;
        } else {
            for (size_t f = 0; f < m.faceCount(); ++f)
                if (sel_[2][f])
                    for (uint32_t k = m.faceStart[f]; k < m.faceStart[f + 1]; ++k) mask[m.faceVerts[k]] = 1;
        }
        for (size_t v = 0; v < mask.size(); ++v)
            if (mask[v]) out.push_back(static_cast<uint32_t>(v));
        return out;
    }

    bool pivot(Vec3f& out) const {
        const std::vector<uint32_t> verts = affectedVertices();
        if (verts.empty()) return false;
        double x = 0, y = 0, z = 0;
        for (uint32_t v : verts) {
            x += mesh_->positions[v].x;
            y += mesh_->positions[v].y;
            z += mesh_->positions[v].z;
        }
        const double n = static_cast<double>(verts.size());
        out = {static_cast<float>(x / n), static_cast<float>(y / n), static_cast<float>(z / n)};
        return true;
    }

    // ---- gizmo
    Gizmo gizmo(float len) const {
        Gizmo g;
        if (!pivot(g.pivot)) return g;
        g.valid = true;
        g.len = len;
        g.tip[0] = add(g.pivot, {len, 0, 0});
        g.tip[1] = add(g.pivot, {0, len, 0});
        g.tip[2] = add(g.pivot, {0, 0, len});
        return g;
    }

    /** Which handle (0 = X, 1 = Y, 2 = Z) is under the finger, or -1. */
    int hitGizmo(const Camera& cam, float x, float y, float radiusPx, float len) const {
        const Gizmo g = gizmo(len);
        if (!g.valid) return -1;
        Vec2 p;
        if (!cam.project(g.pivot, p)) return -1;
        int best = -1;
        float bestScore = std::numeric_limits<float>::max();
        for (int a = 0; a < 3; ++a) {
            Vec2 t;
            if (!cam.project(g.tip[a], t)) continue;
            const float dTip = std::sqrt((t.x - x) * (t.x - x) + (t.y - y) * (t.y - y));
            const float dSeg = distPointSegment(x, y, p, t);
            float score = std::numeric_limits<float>::max();
            if (dTip <= radiusPx) score = dTip;
            else if (dSeg <= radiusPx * 0.5f) score = dSeg * 1.5f + radiusPx * 0.25f;
            if (score < bestScore) {
                bestScore = score;
                best = a;
            }
        }
        return best;
    }

    bool dragging() const { return dragging_; }

    bool beginDrag(const Camera& cam, float x, float y, int axis, Tool tool, float len) {
        if (!mesh_ || axis < 0 || axis > 2 || dragging_) return false;
        dragVerts_ = affectedVertices();
        if (dragVerts_.empty() || !pivot(pivot_)) return false;
        dragOrig_.resize(dragVerts_.size());
        for (size_t i = 0; i < dragVerts_.size(); ++i) dragOrig_[i] = mesh_->positions[dragVerts_[i]];
        axis_ = axis;
        tool_ = tool;
        axisDir_ = axis == 0 ? Vec3f{1, 0, 0} : (axis == 1 ? Vec3f{0, 1, 0} : Vec3f{0, 0, 1});
        len_ = std::max(len, 1e-6f);
        t0_ = 0.0f;
        accAng_ = 0.0f;
        sign_ = dot(axisDir_, sub(cam.eye, pivot_)) >= 0.0f ? 1.0f : -1.0f;
        changed_ = false;
        if (tool == Rotate) {
            if (!cam.project(pivot_, pivotS_)) return false;
            lastAng_ = screenAngle(x, y);
        } else {
            if (!axisParam(cam, x, y, t0_)) return false;
        }
        dragging_ = true;
        return true;
    }

    void updateDrag(const Camera& cam, float x, float y) {
        if (!dragging_) return;
        if (tool_ == Rotate) {
            const float a = screenAngle(x, y);
            accAng_ += wrapPi(a - lastAng_);
            lastAng_ = a;
            applyRotate(accAng_ * sign_);
        } else {
            float t;
            if (!axisParam(cam, x, y, t)) return;
            if (tool_ == Move) applyMove(mulf(axisDir_, t - t0_));
            else applyScale(std::max(0.01f, 1.0f + (t - t0_) / len_));
        }
        changed_ = true;
    }

    void endDrag(bool commit) {
        if (!dragging_) return;
        dragging_ = false;
        if (!commit || !changed_) {
            for (size_t i = 0; i < dragVerts_.size(); ++i) mesh_->positions[dragVerts_[i]] = dragOrig_[i];
            if (changed_) touchGeometry();
            return;
        }
        Entry e;
        e.verts = dragVerts_;
        e.before = dragOrig_;
        e.after.resize(dragVerts_.size());
        for (size_t i = 0; i < dragVerts_.size(); ++i) e.after[i] = mesh_->positions[dragVerts_[i]];
        undoBytes_ += e.bytes();
        undo_.push_back(std::move(e));
        redo_.clear();
        while (undo_.size() > 1 && (undo_.size() > kMaxUndoEntries || undoBytes_ > kMaxUndoBytes)) {
            undoBytes_ -= undo_.front().bytes();
            undo_.pop_front();
        }
    }

    // ---- undo / redo (vertex moves)
    bool canUndo() const { return !undo_.empty(); }
    bool canRedo() const { return !redo_.empty(); }
    size_t undoCount() const { return undo_.size(); }

    bool undo() {
        if (undo_.empty() || dragging_) return false;
        Entry e = std::move(undo_.back());
        undo_.pop_back();
        undoBytes_ -= e.bytes();
        for (size_t i = 0; i < e.verts.size(); ++i) mesh_->positions[e.verts[i]] = e.before[i];
        redo_.push_back(std::move(e));
        touchGeometry();
        return true;
    }
    bool redo() {
        if (redo_.empty() || dragging_) return false;
        Entry e = std::move(redo_.back());
        redo_.pop_back();
        for (size_t i = 0; i < e.verts.size(); ++i) mesh_->positions[e.verts[i]] = e.after[i];
        undoBytes_ += e.bytes();
        undo_.push_back(std::move(e));
        touchGeometry();
        return true;
    }

private:
    struct Entry {
        std::vector<uint32_t> verts;
        std::vector<Vec3f> before, after;
        size_t bytes() const { return verts.size() * (sizeof(uint32_t) + 2 * sizeof(Vec3f)); }
    };

    Mesh* mesh_ = nullptr;
    Topology topo_;
    Elem mode_ = Vertex;
    std::vector<uint8_t> sel_[3];
    uint64_t selVersion_ = 0, geoVersion_ = 0;

    // face cache for visibility / picking
    mutable std::vector<Vec3f> fN_, fC_;
    mutable bool cacheDirty_ = true;

    // drag state
    bool dragging_ = false, changed_ = false;
    int axis_ = 0;
    Tool tool_ = Move;
    Vec3f axisDir_, pivot_;
    float len_ = 1.0f, t0_ = 0.0f, lastAng_ = 0.0f, accAng_ = 0.0f, sign_ = 1.0f;
    Vec2 pivotS_;
    std::vector<uint32_t> dragVerts_;
    std::vector<Vec3f> dragOrig_;

    std::deque<Entry> undo_, redo_;
    size_t undoBytes_ = 0;

    void touchGeometry() {
        cacheDirty_ = true;
        ++geoVersion_;
    }

    void refreshCache() const {
        if (!cacheDirty_ || !mesh_) return;
        const Mesh& m = *mesh_;
        const size_t nf = m.faceCount();
        fN_.assign(nf, Vec3f{0, 0, 0});
        fC_.assign(nf, Vec3f{0, 0, 0});
        for (size_t f = 0; f < nf; ++f) {
            const uint32_t s = m.faceStart[f], e = m.faceStart[f + 1];
            const uint32_t n = e - s;
            if (n == 0) continue;
            Vec3f nrm{0, 0, 0}, c{0, 0, 0};
            for (uint32_t k = 0; k < n; ++k) {
                const Vec3f& a = m.positions[m.faceVerts[s + k]];
                const Vec3f& b = m.positions[m.faceVerts[s + (k + 1) % n]];
                nrm.x += (a.y - b.y) * (a.z + b.z);
                nrm.y += (a.z - b.z) * (a.x + b.x);
                nrm.z += (a.x - b.x) * (a.y + b.y);
                c = add(c, a);
            }
            fN_[f] = normalize(nrm);
            fC_[f] = mulf(c, 1.0f / static_cast<float>(n));
        }
        cacheDirty_ = false;
    }

    bool frontFacing(size_t f, Vec3f eye) const { return dot(fN_[f], sub(eye, fC_[f])) > 0.0f; }

    bool vertexVisible(size_t v, Vec3f eye) const {
        const uint32_t s = topo_.vertFaceStart[v], e = topo_.vertFaceStart[v + 1];
        if (s == e) return true;
        for (uint32_t i = s; i < e; ++i)
            if (frontFacing(topo_.vertFaces[i], eye)) return true;
        return false;
    }

    bool edgeVisible(size_t ed, Vec3f eye) const {
        const uint32_t s = topo_.edgeFaceStart[ed], e = topo_.edgeFaceStart[ed + 1];
        if (s == e) return true;
        for (uint32_t i = s; i < e; ++i)
            if (frontFacing(topo_.edgeFaces[i], eye)) return true;
        return false;
    }

    static float distPointSegment(float px, float py, const Vec2& a, const Vec2& b) {
        const float dx = b.x - a.x, dy = b.y - a.y;
        const float l2 = dx * dx + dy * dy;
        float t = l2 > 0.0f ? ((px - a.x) * dx + (py - a.y) * dy) / l2 : 0.0f;
        t = std::clamp(t, 0.0f, 1.0f);
        const float cx = a.x + t * dx - px, cy = a.y + t * dy - py;
        return std::sqrt(cx * cx + cy * cy);
    }

    static bool insidePoly(const std::vector<Vec2>& poly, float x, float y) {
        bool in = false;
        for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
            const Vec2& a = poly[i];
            const Vec2& b = poly[j];
            if (((a.y > y) != (b.y > y)) && (x < (b.x - a.x) * (y - a.y) / (b.y - a.y) + a.x)) in = !in;
        }
        return in;
    }

    int pickVertex(const Camera& cam, float x, float y, float r) const {
        int best = -1;
        float bestD = r * r;
        for (size_t v = 0; v < mesh_->vertexCount(); ++v) {
            Vec2 s;
            if (!cam.project(mesh_->positions[v], s)) continue;
            const float d = (s.x - x) * (s.x - x) + (s.y - y) * (s.y - y);
            if (d >= bestD || !vertexVisible(v, cam.eye)) continue;
            bestD = d;
            best = static_cast<int>(v);
        }
        return best;
    }

    int pickEdge(const Camera& cam, float x, float y, float r) const {
        int best = -1;
        float bestD = r;
        for (size_t e = 0; e < topo_.edgeCount(); ++e) {
            Vec2 a, b;
            if (!cam.project(mesh_->positions[topo_.edgeA[e]], a) || !cam.project(mesh_->positions[topo_.edgeB[e]], b))
                continue;
            const float d = distPointSegment(x, y, a, b);
            if (d >= bestD || !edgeVisible(e, cam.eye)) continue;
            bestD = d;
            best = static_cast<int>(e);
        }
        return best;
    }

    int pickFace(const Camera& cam, float x, float y) const {
        Vec3f o, d;
        if (!cam.ray(x, y, o, d)) return -1;
        const Mesh& m = *mesh_;
        int best = -1;
        double bestT = std::numeric_limits<double>::max();
        for (size_t f = 0; f < m.faceCount(); ++f) {
            if (!frontFacing(f, cam.eye)) continue;
            const uint32_t s = m.faceStart[f], e = m.faceStart[f + 1];
            for (uint32_t k = s + 1; k + 1 < e; ++k) {
                double t;
                if (rayTriangle(o, d, m.positions[m.faceVerts[s]], m.positions[m.faceVerts[k]],
                                m.positions[m.faceVerts[k + 1]], t) &&
                    t < bestT) {
                    bestT = t;
                    best = static_cast<int>(f);
                }
            }
        }
        return best;
    }

    // Moeller-Trumbore, double precision.
    static bool rayTriangle(Vec3f o, Vec3f d, Vec3f a, Vec3f b, Vec3f c, double& tOut) {
        const double e1[3] = {double(b.x) - a.x, double(b.y) - a.y, double(b.z) - a.z};
        const double e2[3] = {double(c.x) - a.x, double(c.y) - a.y, double(c.z) - a.z};
        const double dd[3] = {d.x, d.y, d.z};
        const double p[3] = {dd[1] * e2[2] - dd[2] * e2[1], dd[2] * e2[0] - dd[0] * e2[2], dd[0] * e2[1] - dd[1] * e2[0]};
        const double det = e1[0] * p[0] + e1[1] * p[1] + e1[2] * p[2];
        if (std::fabs(det) < 1e-14) return false;
        const double inv = 1.0 / det;
        const double tv[3] = {double(o.x) - a.x, double(o.y) - a.y, double(o.z) - a.z};
        const double u = (tv[0] * p[0] + tv[1] * p[1] + tv[2] * p[2]) * inv;
        if (u < 0.0 || u > 1.0) return false;
        const double q[3] = {tv[1] * e1[2] - tv[2] * e1[1], tv[2] * e1[0] - tv[0] * e1[2], tv[0] * e1[1] - tv[1] * e1[0]};
        const double v = (dd[0] * q[0] + dd[1] * q[1] + dd[2] * q[2]) * inv;
        if (v < 0.0 || u + v > 1.0) return false;
        const double t = (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) * inv;
        if (t <= 0.0) return false;
        tOut = t;
        return true;
    }

    // Parameter t of the point on the axis line (pivot + t * axis) closest to the ray through pixel (x, y).
    bool axisParam(const Camera& cam, float x, float y, float& t) const {
        Vec3f o, r;
        if (!cam.ray(x, y, o, r)) return false;
        const Vec3f w0 = sub(pivot_, o);
        const float b = dot(axisDir_, r), d = dot(axisDir_, w0), e = dot(r, w0);
        const float denom = 1.0f - b * b;
        if (denom < 1e-5f) return false;  // axis parallel to the view ray
        t = (b * e - d) / denom;
        return std::isfinite(t);
    }

    float screenAngle(float x, float y) const { return std::atan2(-(y - pivotS_.y), x - pivotS_.x); }

    static float wrapPi(float a) {
        const float kPi = 3.14159265358979f;
        while (a > kPi) a -= 2.0f * kPi;
        while (a < -kPi) a += 2.0f * kPi;
        return a;
    }

    void applyMove(Vec3f delta) {
        for (size_t i = 0; i < dragVerts_.size(); ++i) mesh_->positions[dragVerts_[i]] = add(dragOrig_[i], delta);
        touchGeometry();
    }
    void applyScale(float factor) {
        for (size_t i = 0; i < dragVerts_.size(); ++i) {
            const Vec3f rel = sub(dragOrig_[i], pivot_);
            const float along = dot(rel, axisDir_);
            mesh_->positions[dragVerts_[i]] = add(pivot_, add(rel, mulf(axisDir_, (factor - 1.0f) * along)));
        }
        touchGeometry();
    }
    void applyRotate(float angle) {
        const float c = std::cos(angle), s = std::sin(angle);
        for (size_t i = 0; i < dragVerts_.size(); ++i) {
            const Vec3f v = sub(dragOrig_[i], pivot_);
            // Rodrigues: v' = v c + (k x v) s + k (k . v) (1 - c)
            const Vec3f r = add(add(mulf(v, c), mulf(cross(axisDir_, v), s)), mulf(axisDir_, dot(axisDir_, v) * (1.0f - c)));
            mesh_->positions[dragVerts_[i]] = add(pivot_, r);
        }
        touchGeometry();
    }
};

}  // namespace edit
