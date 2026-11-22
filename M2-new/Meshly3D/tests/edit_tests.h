// Editing-core tests. Included by loader_test.cpp AFTER the CHECK macro is defined.
#pragma once
#include <cmath>
#include <vector>

#include "editmesh.h"

namespace edit_tests {

using edit::Camera;
using edit::EditSession;
using edit::Vec2;

inline Camera makeCam(Vec3f eye, int w = 800, int h = 600) {
    float proj[16], view[16], vp[16];
    m3::perspective(proj, 50.0f * 3.14159265f / 180.0f, float(w) / float(h), 0.05f, 100.0f);
    m3::lookAt(view, eye, {0, 0, 0}, {0, 1, 0});
    m3::multiply(vp, proj, view);
    Camera c;
    c.setup(vp, w, h, eye);
    return c;
}

inline Vec2 P(const Camera& c, Vec3f p) {
    Vec2 s;
    c.project(p, s);
    return s;
}

inline bool nearf(float a, float b, float eps) { return std::fabs(a - b) <= eps; }

inline float screenAngle(const Vec2& center, const Vec2& p) { return std::atan2(-(p.y - center.y), p.x - center.x); }

inline float wrap(float a) {
    const float kPi = 3.14159265f;
    while (a > kPi) a -= 2 * kPi;
    while (a < -kPi) a += 2 * kPi;
    return a;
}

inline std::vector<Vec2> fullScreen(const Camera& c) {
    return {{0, 0}, {float(c.width), 0}, {float(c.width), float(c.height)}, {0, float(c.height)}};
}

// Selects the top (+Y) face of the unit cube by tapping its centre.
inline void selectTopFace(EditSession& s, const Camera& cam) {
    s.setElem(EditSession::Face);
    const Vec2 c = P(cam, {0, 0.5f, 0});
    s.tapSelect(cam, c.x, c.y, 30.0f, false);
}

inline void runEditTests(const std::vector<uint8_t>& torusGlb) {
    // ---------------------------------------------------------------- topology
    {
        Mesh m = Mesh::makeCube();
        edit::Topology t;
        t.build(m);
        CHECK(t.edgeCount() == 12);
        CHECK(t.faceEdges.size() == 24);
        for (size_t e = 0; e < t.edgeCount(); ++e)
            CHECK(t.edgeFaceStart[e + 1] - t.edgeFaceStart[e] == 2);  // closed cube: every edge has 2 faces
        for (size_t v = 0; v < 8; ++v) CHECK(t.vertFaceStart[v + 1] - t.vertFaceStart[v] == 3);
        for (size_t f = 0; f < 6; ++f) {
            std::vector<uint32_t> es(t.faceEdges.begin() + m.faceStart[f], t.faceEdges.begin() + m.faceStart[f + 1]);
            std::sort(es.begin(), es.end());
            CHECK(std::unique(es.begin(), es.end()) == es.end());  // 4 different edges per face
        }
    }

    // ---------------------------------------------------------------- camera
    {
        const Camera cam = makeCam({3, 2.5f, 4});
        const Vec3f pts[3] = {{0, 0, 0}, {0.5f, 0.5f, 0.5f}, {-1, 0.3f, 0.7f}};
        for (const Vec3f& p : pts) {
            Vec2 s;
            CHECK(cam.project(p, s));
            Vec3f o, d;
            CHECK(cam.ray(s.x, s.y, o, d));
            const Vec3f w = m3::sub(p, o);
            const Vec3f perp = m3::sub(w, m3::mulf(d, m3::dot(w, d)));
            CHECK(m3::length(perp) < 1e-3f);  // the ray passes through the point
        }
        Vec2 s;
        CHECK(!cam.project({3, 2.5f, 40}, s));  // behind the camera
    }

    // ---------------------------------------------------------------- selection conversion + picking
    {
        Mesh m = Mesh::makeCube();
        EditSession s;
        s.attach(&m);
        const Camera cam = makeCam({3, 2.5f, 4});
        // top face vertices: 7, 6, 2, 3 (all visible)
        for (uint32_t v : {7u, 6u, 2u, 3u}) {
            const Vec2 sp = P(cam, m.positions[v]);
            CHECK(s.tapSelect(cam, sp.x, sp.y, 25.0f, true));
        }
        CHECK(s.selectedCount() == 4);
        s.setElem(EditSession::Edge);
        CHECK(s.selectedCount() == 4);  // the 4 edges of the top ring
        s.setElem(EditSession::Face);
        CHECK(s.selectedCount() == 1);
        CHECK(s.isSelected(EditSession::Face, 4));
        s.setElem(EditSession::Vertex);
        CHECK(s.selectedCount() == 4);
        for (uint32_t v : {7u, 6u, 2u, 3u}) CHECK(s.isSelected(EditSession::Vertex, v));

        // single tap replaces; extend toggles; empty space clears
        const Vec2 p6 = P(cam, m.positions[6]);
        CHECK(s.tapSelect(cam, p6.x, p6.y, 25.0f, false));
        CHECK(s.selectedCount() == 1 && s.isSelected(EditSession::Vertex, 6));
        CHECK(s.tapSelect(cam, p6.x, p6.y, 25.0f, true));  // toggled off
        CHECK(s.selectedCount() == 0);
        CHECK(s.tapSelect(cam, p6.x, p6.y, 25.0f, false));
        CHECK(!s.tapSelect(cam, 5.0f, 5.0f, 25.0f, false));  // far from everything
        CHECK(s.selectedCount() == 0);
    }
    {
        Mesh m = Mesh::makeCube();
        EditSession s;
        s.attach(&m);
        const Camera cam = makeCam({3, 2.5f, 4});
        s.setElem(EditSession::Edge);
        const Vec2 mid = P(cam, {0, 0.5f, 0.5f});  // middle of edge (6, 7)
        CHECK(s.tapSelect(cam, mid.x, mid.y, 25.0f, false));
        CHECK(s.selectedCount() == 1);
        bool found = false;
        for (size_t e = 0; e < s.topology().edgeCount(); ++e)
            if (s.isSelected(EditSession::Edge, e)) found = s.topology().edgeA[e] == 6 && s.topology().edgeB[e] == 7;
        CHECK(found);

        s.setElem(EditSession::Face);
        const Vec2 cx = P(cam, {0.5f, 0, 0});  // +X face centre
        CHECK(s.tapSelect(cam, cx.x, cx.y, 25.0f, false));
        CHECK(s.isSelected(EditSession::Face, 2));
        // a hidden vertex (0 = -,-,-) can not be picked
        s.setElem(EditSession::Vertex);
        s.clearSelection();
        const Vec2 p0 = P(cam, m.positions[0]);
        s.tapSelect(cam, p0.x, p0.y, 5.0f, false);
        CHECK(!s.isSelected(EditSession::Vertex, 0));
    }

    // ---------------------------------------------------------------- box / lasso
    {
        Mesh m = Mesh::makeCube();
        EditSession s;
        s.attach(&m);
        const Camera cam = makeCam({3, 2.5f, 4});
        s.regionSelect(cam, fullScreen(cam), false);
        CHECK(s.selectedCount() == 7);  // everything except the far corner
        CHECK(!s.isSelected(EditSession::Vertex, 0));
        s.setElem(EditSession::Face);
        s.clearSelection();
        s.regionSelect(cam, fullScreen(cam), false);
        CHECK(s.selectedCount() == 3);  // +X, +Y, +Z
        s.setElem(EditSession::Edge);
        s.clearSelection();
        s.regionSelect(cam, fullScreen(cam), false);
        CHECK(s.selectedCount() == 9);
        // lasso (triangle) around one vertex only
        s.setElem(EditSession::Vertex);
        s.clearSelection();
        const Vec2 p = P(cam, m.positions[6]);
        std::vector<Vec2> tri = {{p.x - 20, p.y + 20}, {p.x + 20, p.y + 20}, {p.x, p.y - 25}};
        s.regionSelect(cam, tri, false);
        CHECK(s.selectedCount() == 1 && s.isSelected(EditSession::Vertex, 6));
    }

    // ---------------------------------------------------------------- gizmo: move (exact tracking), undo / redo
    {
        Mesh m = Mesh::makeCube();
        const std::vector<Vec3f> orig = m.positions;
        EditSession s;
        s.attach(&m);
        const Camera cam = makeCam({3, 2.5f, 4});
        selectTopFace(s, cam);
        CHECK(s.isSelected(EditSession::Face, 4));
        const float len = 1.0f;
        const EditSession::Gizmo g = s.gizmo(len);
        CHECK(g.valid && nearf(g.pivot.y, 0.5f, 1e-5f) && nearf(g.pivot.x, 0.0f, 1e-5f));
        const Vec2 tipS = P(cam, g.tip[0]);
        CHECK(s.hitGizmo(cam, tipS.x, tipS.y, 40.0f, len) == 0);
        CHECK(s.hitGizmo(cam, 3.0f, 3.0f, 40.0f, len) == -1);
        CHECK(s.beginDrag(cam, tipS.x, tipS.y, 0, EditSession::Move, len));
        const Vec2 toS = P(cam, m3::add(g.tip[0], {0.37f, 0, 0}));
        s.updateDrag(cam, toS.x, toS.y);
        for (uint32_t v : {2u, 3u, 6u, 7u}) {
            CHECK(nearf(m.positions[v].x - orig[v].x, 0.37f, 2e-3f));
            CHECK(nearf(m.positions[v].y, orig[v].y, 1e-5f) && nearf(m.positions[v].z, orig[v].z, 1e-5f));
        }
        for (uint32_t v : {0u, 1u, 4u, 5u}) CHECK(m.positions[v].x == orig[v].x);  // unselected stay put
        s.endDrag(true);
        CHECK(s.undoCount() == 1 && s.canUndo() && !s.canRedo());
        const std::vector<Vec3f> moved = m.positions;
        CHECK(s.undo());
        bool same = true;
        for (size_t i = 0; i < orig.size(); ++i) same = same && m.positions[i].x == orig[i].x && m.positions[i].y == orig[i].y && m.positions[i].z == orig[i].z;
        CHECK(same);
        CHECK(s.canRedo());
        CHECK(s.redo());
        bool again = true;
        for (size_t i = 0; i < orig.size(); ++i) again = again && m.positions[i].x == moved[i].x && m.positions[i].z == moved[i].z;
        CHECK(again);
        // cancel restores
        const std::vector<Vec3f> before = m.positions;
        selectTopFace(s, cam);
        const EditSession::Gizmo g2 = s.gizmo(len);
        const Vec2 t2 = P(cam, g2.tip[1]);
        CHECK(s.beginDrag(cam, t2.x, t2.y, 1, EditSession::Move, len));
        const Vec2 t3 = P(cam, m3::add(g2.tip[1], {0, 0.8f, 0}));
        s.updateDrag(cam, t3.x, t3.y);
        s.endDrag(false);
        bool restored = true;
        for (size_t i = 0; i < before.size(); ++i) restored = restored && m.positions[i].y == before[i].y;
        CHECK(restored);
    }

    // ---------------------------------------------------------------- gizmo: scale
    {
        Mesh m = Mesh::makeCube();
        const std::vector<Vec3f> orig = m.positions;
        EditSession s;
        s.attach(&m);
        const Camera cam = makeCam({3, 2.5f, 4});
        selectTopFace(s, cam);
        const float len = 1.0f;
        const EditSession::Gizmo g = s.gizmo(len);
        const Vec2 a = P(cam, g.tip[0]);
        CHECK(s.beginDrag(cam, a.x, a.y, 0, EditSession::Scale, len));
        const Vec2 b = P(cam, m3::add(g.tip[0], {0.5f * len, 0, 0}));  // outward by half a handle: factor 1.5
        s.updateDrag(cam, b.x, b.y);
        for (uint32_t v : {2u, 3u, 6u, 7u})
            CHECK(nearf(m.positions[v].x, g.pivot.x + 1.5f * (orig[v].x - g.pivot.x), 3e-3f));
        s.endDrag(true);
    }

    // ---------------------------------------------------------------- gizmo: rotate direction
    // The model must turn the same way as the finger, whether the axis points towards or away from the viewer.
    for (const float ex : {3.0f, -3.0f})
        for (int axis = 0; axis < 3; ++axis) {
            Mesh m = Mesh::makeCube();
            const std::vector<Vec3f> orig = m.positions;
            EditSession s;
            s.attach(&m);
            const Camera cam = makeCam({ex, 2.5f, 4});
            selectTopFace(s, cam);
            CHECK(s.isSelected(EditSession::Face, 4));
            const float len = 1.0f;
            const EditSession::Gizmo g = s.gizmo(len);
            const Vec3f ax = axis == 0 ? Vec3f{1, 0, 0} : (axis == 1 ? Vec3f{0, 1, 0} : Vec3f{0, 0, 1});
            // vertex 6 circles around the axis line; the centre of that circle is its projection onto the axis
            const Vec3f rel = m3::sub(orig[6], g.pivot);
            const Vec2 centre = P(cam, m3::add(g.pivot, m3::mulf(ax, m3::dot(rel, ax))));
            const float a0 = screenAngle(centre, P(cam, orig[6]));

            const Vec2 pc = P(cam, g.pivot);
            CHECK(s.beginDrag(cam, pc.x + 100, pc.y, axis, EditSession::Rotate, len));
            const float ang = 30.0f * 3.14159265f / 180.0f;
            s.updateDrag(cam, pc.x + 100 * std::cos(ang), pc.y - 100 * std::sin(ang));  // finger turns counter-clockwise
            const float delta = wrap(screenAngle(centre, P(cam, m.positions[6])) - a0);
            CHECK(delta > 0.02f && delta < 1.8f);
            for (uint32_t v : {2u, 3u, 6u, 7u})  // rotation keeps the position along the axis
                CHECK(nearf(m3::dot(m3::sub(m.positions[v], g.pivot), ax), m3::dot(m3::sub(orig[v], g.pivot), ax), 1e-4f));
            for (uint32_t v : {0u, 1u, 4u, 5u}) CHECK(m.positions[v].x == orig[v].x && m.positions[v].z == orig[v].z);
            s.endDrag(true);
        }

    // ---------------------------------------------------------------- undo memory limit
    {
        Mesh m = Mesh::makeCube();
        EditSession s;
        s.attach(&m);
        const Camera cam = makeCam({3, 2.5f, 4});
        selectTopFace(s, cam);
        const float len = 1.0f;
        for (int i = 0; i < 300; ++i) {
            const EditSession::Gizmo g = s.gizmo(len);
            const Vec2 a = P(cam, g.tip[0]);
            if (!s.beginDrag(cam, a.x, a.y, 0, EditSession::Move, len)) break;
            const Vec2 b = P(cam, m3::add(g.tip[0], {0.01f, 0, 0}));
            s.updateDrag(cam, b.x, b.y);
            s.endDrag(true);
        }
        CHECK(s.undoCount() <= EditSession::kMaxUndoEntries);
        CHECK(s.undoCount() >= 100);
    }

    // ---------------------------------------------------------------- fuzz on a real model (run with sanitizers)
    {
        Mesh m;
        CHECK(loadGlb(torusGlb.data(), torusGlb.size(), 700000, m).empty());
        const std::vector<Vec3f> orig = m.positions;
        EditSession s;
        s.attach(&m);
        unsigned long long st = 0x9E3779B97F4A7C15ULL;
        auto rnd = [&]() {
            st ^= st << 13;
            st ^= st >> 7;
            st ^= st << 17;
            return st;
        };
        auto rf = [&](float lo, float hi) { return lo + (hi - lo) * float(rnd() % 100000) / 100000.0f; };
        for (int it = 0; it < 400; ++it) {
            const float yaw = rf(0, 6.28f), pitch = rf(-1.2f, 1.2f), dist = rf(2.0f, 6.0f);
            const Camera cam = makeCam({dist * std::cos(pitch) * std::sin(yaw), dist * std::sin(pitch), dist * std::cos(pitch) * std::cos(yaw)});
            s.setElem(EditSession::Elem(rnd() % 3));
            switch (rnd() % 4) {
                case 0: s.tapSelect(cam, rf(0, 800), rf(0, 600), 30, rnd() % 2 == 0); break;
                case 1: {
                    const float x = rf(0, 700), y = rf(0, 500);
                    s.regionSelect(cam, {{x, y}, {x + rf(10, 300), y}, {x + rf(10, 300), y + rf(10, 300)}, {x, y + rf(10, 300)}}, rnd() % 2 == 0);
                    break;
                }
                case 2: s.selectAll(); break;
                default: s.clearSelection(); break;
            }
            if (s.hasSelection() && rnd() % 2 == 0) {
                const EditSession::Gizmo g = s.gizmo(1.0f);
                const int axis = int(rnd() % 3);
                const Vec2 a = P(cam, g.tip[axis]);
                if (s.beginDrag(cam, a.x, a.y, axis, EditSession::Tool(rnd() % 3), 1.0f)) {
                    for (int k = 0; k < 5; ++k) s.updateDrag(cam, a.x + rf(-200, 200), a.y + rf(-200, 200));
                    s.endDrag(rnd() % 4 != 0);
                }
            }
            if (rnd() % 5 == 0) s.undo();
            if (rnd() % 7 == 0) s.redo();
        }
        while (s.undo()) {
        }
        bool exact = true;
        for (size_t i = 0; i < orig.size(); ++i)
            exact = exact && m.positions[i].x == orig[i].x && m.positions[i].y == orig[i].y && m.positions[i].z == orig[i].z;
        CHECK(exact);  // undoing everything gives back the exact original model
    }
}

}  // namespace edit_tests
