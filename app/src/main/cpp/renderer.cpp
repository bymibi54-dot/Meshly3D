#include "renderer.h"

#include <GLES3/gl3.h>
#include <android/log.h>

#include <algorithm>
#include <cmath>
#include <sstream>
#include <vector>

#include "glb_loader.h"
#include "mathutil.h"

#define LOG_TAG "Meshly3D"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

using m3::add;
using m3::mulf;
using m3::normalize;
using m3::sub;

namespace {

const float kFovDeg = 50.0f;
const float kDeg2Rad = 3.14159265358979f / 180.0f;

const char* kMeshVs = R"(#version 300 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
uniform mat4 uMvp;
out vec3 vNormal;
void main() {
    vNormal = aNormal;
    gl_Position = uMvp * vec4(aPos, 1.0);
}
)";

const char* kMeshFs = R"(#version 300 es
precision mediump float;
in vec3 vNormal;
uniform vec3 uLight;
out vec4 outColor;
void main() {
    vec3 n = normalize(vNormal);
    float d = max(dot(n, normalize(uLight)), 0.0);
    vec3 base = vec3(0.62, 0.66, 0.72);
    outColor = vec4(base * (0.32 + 0.68 * d), 1.0);
}
)";

const char* kLineVs = R"(#version 300 es
layout(location = 0) in vec3 aPos;
uniform mat4 uMvp;
void main() {
    gl_Position = uMvp * vec4(aPos, 1.0);
}
)";

const char* kLineFs = R"(#version 300 es
precision mediump float;
uniform vec4 uColor;
out vec4 outColor;
void main() {
    outColor = uColor;
}
)";

const char* kPointVs = R"(#version 300 es
layout(location = 0) in vec3 aPos;
uniform mat4 uMvp;
uniform float uSize;
void main() {
    gl_Position = uMvp * vec4(aPos, 1.0);
    gl_PointSize = uSize;
}
)";

const char* kPointFs = R"(#version 300 es
precision mediump float;
uniform vec4 uColor;
out vec4 outColor;
void main() {
    vec2 c = gl_PointCoord * 2.0 - 1.0;
    if (dot(c, c) > 1.0) discard;
    outColor = uColor;
}
)";

GLuint compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[512];
        glGetShaderInfoLog(s, sizeof(buf), nullptr, buf);
        LOGE("Shader compile error: %s", buf);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

GLuint link(const char* vs, const char* fs) {
    GLuint v = compile(GL_VERTEX_SHADER, vs);
    GLuint f = compile(GL_FRAGMENT_SHADER, fs);
    if (!v || !f) return 0;
    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glDeleteShader(v);
    glDeleteShader(f);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[512];
        glGetProgramInfoLog(p, sizeof(buf), nullptr, buf);
        LOGE("Program link error: %s", buf);
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

// Creates (or refills) a VAO+VBO of tightly packed floats: attribute 0 = position, optional attribute 1 = normal.
void fillBuffer(unsigned int& vao, unsigned int& vbo, int& count, const std::vector<float>& data, bool withNormal,
                GLenum usage = GL_STATIC_DRAW) {
    if (!vao) glGenVertexArrays(1, &vao);
    if (!vbo) glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.size() * sizeof(float)), data.data(), usage);
    const GLsizei stride = (withNormal ? 6 : 3) * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(0));
    if (withNormal) {
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(3 * sizeof(float)));
    }
    glBindVertexArray(0);
    count = static_cast<int>(data.size() / (withNormal ? 6 : 3));
}

void pushPos(std::vector<float>& v, const Vec3f& p) {
    v.push_back(p.x);
    v.push_back(p.y);
    v.push_back(p.z);
}

const float kIdentity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

}  // namespace

// ================================================================================== setup / camera

Renderer::Renderer() {
    mesh_ = Mesh::makeCube();
    edit_.attach(&mesh_);
    computeFrame();
}

void Renderer::computeFrame() {
    Vec3f mn, mx;
    if (!mesh_.bounds(mn, mx)) {
        center_[0] = center_[1] = center_[2] = 0.0f;
        radius_ = 1.0f;
    } else {
        center_[0] = (mn.x + mx.x) * 0.5f;
        center_[1] = (mn.y + mx.y) * 0.5f;
        center_[2] = (mn.z + mx.z) * 0.5f;
        const float dx = mx.x - mn.x, dy = mx.y - mn.y, dz = mx.z - mn.z;
        radius_ = std::max(0.5f * std::sqrt(dx * dx + dy * dy + dz * dz), 1e-4f);
    }
    for (int i = 0; i < 3; ++i) target_[i] = center_[i];
    dist_ = radius_ * 2.8f;
    // Grid cell size: a power of ten, so that the grid is always larger than the model.
    const float want = std::max(radius_ * 0.4f, 1e-6f);
    gridStep_ = std::pow(10.0f, std::ceil(std::log10(want)));
}

void Renderer::viewProj(float* vp, Vec3f& eye) const {
    const float yaw = yaw_ * kDeg2Rad;
    const float pitch = pitch_ * kDeg2Rad;
    const Vec3f tgt = {target_[0], target_[1], target_[2]};
    eye = {tgt.x + dist_ * std::cos(pitch) * std::sin(yaw), tgt.y + dist_ * std::sin(pitch),
           tgt.z + dist_ * std::cos(pitch) * std::cos(yaw)};
    float proj[16], view[16];
    m3::perspective(proj, kFovDeg * kDeg2Rad, static_cast<float>(width_) / static_cast<float>(height_),
                    std::max(dist_ * 0.01f, 1e-5f), dist_ * 100.0f);
    m3::lookAt(view, eye, tgt, {0, 1, 0});
    m3::multiply(vp, proj, view);
}

edit::Camera Renderer::makeCamera() const {
    float vp[16];
    Vec3f eye;
    viewProj(vp, eye);
    edit::Camera cam;
    cam.setup(vp, width_, height_, eye);
    return cam;
}

void Renderer::frame() {
    computeFrame();
    if (glReady_) buildGrid();
}

void Renderer::orbit(float dx, float dy) {
    yaw_ -= dx * 0.4f;
    pitch_ = std::clamp(pitch_ + dy * 0.4f, -89.0f, 89.0f);
}

void Renderer::pan(float dx, float dy) {
    const float yaw = yaw_ * kDeg2Rad;
    const float pitch = pitch_ * kDeg2Rad;
    // Camera looks from the eye toward the target: forward = -(direction target -> eye).
    const Vec3f fwd = normalize({-std::cos(pitch) * std::sin(yaw), -std::sin(pitch), -std::cos(pitch) * std::cos(yaw)});
    const Vec3f right = normalize(m3::cross(fwd, {0, 1, 0}));
    const Vec3f up = m3::cross(right, fwd);
    const float perPixel = 2.0f * dist_ * std::tan(kFovDeg * kDeg2Rad * 0.5f) / static_cast<float>(height_);
    target_[0] += -right.x * dx * perPixel + up.x * dy * perPixel;
    target_[1] += -right.y * dx * perPixel + up.y * dy * perPixel;
    target_[2] += -right.z * dx * perPixel + up.z * dy * perPixel;
}

void Renderer::zoom(float scale) {
    if (!(scale > 0.0f)) return;
    dist_ = std::clamp(dist_ / scale, radius_ * 0.05f, radius_ * 50.0f);
}

// ================================================================================== model

void Renderer::resetToCube() {
    touchCancel();
    mesh_ = Mesh::makeCube();
    edit_.attach(&mesh_);
    computeFrame();
    markAllDirty();
    if (glReady_) buildGrid();
}

std::string Renderer::loadGlb(const uint8_t* data, size_t size) {
    Mesh m;
    const std::string err = ::loadGlb(data, size, kDefaultMaxTriangles, m);
    if (!err.empty()) return err;
    touchCancel();
    mesh_ = std::move(m);
    edit_.attach(&mesh_);
    computeFrame();
    markAllDirty();
    if (glReady_) buildGrid();
    return "";
}

std::string Renderer::stats() const {
    std::ostringstream os;
    os << "V " << mesh_.vertexCount() << " | F " << mesh_.faceCount() << " | T " << mesh_.triangleCount();
    if (editMode_) {
        static const char* kNames[3] = {"verts", "edges", "faces"};
        os << " | Sel " << edit_.selectedCount() << " " << kNames[edit_.elem()];
        os << " | Undo " << edit_.undoCount();
    }
    return os.str();
}

// ================================================================================== editing commands

void Renderer::setEditMode(bool on) {
    touchCancel();
    editMode_ = on;
}
void Renderer::setElement(int e) {
    touchCancel();
    edit_.setElem(static_cast<edit::EditSession::Elem>(std::clamp(e, 0, 2)));
}
void Renderer::setMethod(int m) { method_ = std::clamp(m, 0, 2); }
void Renderer::setExtend(bool on) { extend_ = on; }
void Renderer::setTool(int t) { tool_ = static_cast<edit::EditSession::Tool>(std::clamp(t, 0, 2)); }
void Renderer::undo() {
    touchCancel();
    edit_.undo();
}
void Renderer::redo() {
    touchCancel();
    edit_.redo();
}
void Renderer::selectAll() { edit_.selectAll(); }
void Renderer::selectNone() { edit_.clearSelection(); }
void Renderer::markAllDirty() {
    meshDirty_ = true;
    builtSel_ = ~0ull;
}

// ================================================================================== touch handling

void Renderer::touchDown(float x, float y) {
    touchCancel();
    startX_ = lastX_ = x;
    startY_ = lastY_ = y;
    if (!editMode_) {
        touch_ = Touch::Orbit;
        return;
    }
    if (edit_.hasSelection()) {
        const edit::Camera cam = makeCamera();
        const int axis = edit_.hitGizmo(cam, x, y, 26.0f * density_, gizmoLen());
        if (axis >= 0 && edit_.beginDrag(cam, x, y, axis, tool_, gizmoLen())) {
            touch_ = Touch::Gizmo;
            activeAxis_ = axis;
            return;
        }
    }
    touch_ = Touch::Pending;
}

void Renderer::touchMove(float x, float y) {
    switch (touch_) {
        case Touch::Orbit:
            orbit(x - lastX_, y - lastY_);
            break;
        case Touch::Gizmo:
            edit_.updateDrag(makeCamera(), x, y);
            break;
        case Touch::Pending: {
            const float dx = x - startX_, dy = y - startY_;
            if (dx * dx + dy * dy > (8.0f * density_) * (8.0f * density_)) {
                if (method_ == 0) {
                    touch_ = Touch::Orbit;
                    orbit(x - lastX_, y - lastY_);
                } else {
                    touch_ = Touch::Region;
                    region_pts_.clear();
                    region_pts_.push_back({startX_, startY_});
                }
            }
            break;
        }
        case Touch::Region:
            if (method_ == 1) {
                region_pts_ = {{startX_, startY_}, {x, startY_}, {x, y}, {startX_, y}};
            } else {
                const edit::Vec2& l = region_pts_.back();
                if ((x - l.x) * (x - l.x) + (y - l.y) * (y - l.y) > (6.0f * density_) * (6.0f * density_))
                    region_pts_.push_back({x, y});
            }
            break;
        case Touch::None:
            break;
    }
    lastX_ = x;
    lastY_ = y;
}

void Renderer::touchUp(float x, float y) {
    switch (touch_) {
        case Touch::Pending:
            edit_.tapSelect(makeCamera(), x, y, 22.0f * density_, extend_);
            break;
        case Touch::Region:
            edit_.regionSelect(makeCamera(), region_pts_, extend_);
            break;
        case Touch::Gizmo:
            edit_.endDrag(true);
            break;
        default:
            break;
    }
    touch_ = Touch::None;
    activeAxis_ = -1;
    region_pts_.clear();
}

void Renderer::touchCancel() {
    if (touch_ == Touch::Gizmo) edit_.endDrag(false);
    touch_ = Touch::None;
    activeAxis_ = -1;
    region_pts_.clear();
}

// ================================================================================== GL resources

void Renderer::onSurfaceCreated() {
    // A new GL context was created: all old object names are invalid.
    mesh_buf_ = wire_ = grid_ = allPoints_ = selFaces_ = selEdges_ = selPoints_ = gizmoLines_ = gizmoTips_ = region_ = Buf();
    meshProg_ = lineProg_ = pointProg_ = 0;

    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    const char* gpu = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    std::istringstream vs(version ? version : "?");
    std::string t1, t2, t3;
    vs >> t1 >> t2 >> t3;  // "OpenGL ES 3.2 V@..." -> keep "OpenGL ES 3.2"
    info_ = (t3.empty() ? std::string(version ? version : "?") : t1 + " " + t2 + " " + t3) + " | " + (gpu ? gpu : "?");
    LOGI("GL_VERSION: %s", version ? version : "?");
    LOGI("GL_RENDERER: %s", gpu ? gpu : "?");

    meshProg_ = link(kMeshVs, kMeshFs);
    lineProg_ = link(kLineVs, kLineFs);
    pointProg_ = link(kPointVs, kPointFs);
    if (meshProg_) {
        uMeshMvp_ = glGetUniformLocation(meshProg_, "uMvp");
        uMeshLight_ = glGetUniformLocation(meshProg_, "uLight");
    }
    if (lineProg_) {
        uLineMvp_ = glGetUniformLocation(lineProg_, "uMvp");
        uLineColor_ = glGetUniformLocation(lineProg_, "uColor");
    }
    if (pointProg_) {
        uPointMvp_ = glGetUniformLocation(pointProg_, "uMvp");
        uPointColor_ = glGetUniformLocation(pointProg_, "uColor");
        uPointSize_ = glGetUniformLocation(pointProg_, "uSize");
    }
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glReady_ = true;
    markAllDirty();
    buildGrid();
}

void Renderer::onSurfaceChanged(int width, int height) {
    width_ = std::max(width, 1);
    height_ = std::max(height, 1);
    glViewport(0, 0, width_, height_);
}

void Renderer::uploadMesh() {
    // Filled triangles with flat normals (faces are fanned into triangles).
    std::vector<float> tri;
    tri.reserve(mesh_.triangleCount() * 18);
    for (size_t f = 0; f < mesh_.faceCount(); ++f) {
        const uint32_t s = mesh_.faceStart[f], e = mesh_.faceStart[f + 1];
        const uint32_t n = e - s;
        if (n < 3) continue;
        const uint32_t* v = &mesh_.faceVerts[s];
        Vec3f nrm = {0, 0, 0};  // Newell's method: works for any planar polygon
        for (uint32_t k = 0; k < n; ++k) {
            const Vec3f& a = mesh_.positions[v[k]];
            const Vec3f& b = mesh_.positions[v[(k + 1) % n]];
            nrm.x += (a.y - b.y) * (a.z + b.z);
            nrm.y += (a.z - b.z) * (a.x + b.x);
            nrm.z += (a.x - b.x) * (a.y + b.y);
        }
        nrm = normalize(nrm);
        for (uint32_t k = 1; k + 1 < n; ++k) {
            const uint32_t corner[3] = {v[0], v[k], v[k + 1]};
            for (uint32_t c : corner) {
                pushPos(tri, mesh_.positions[c]);
                pushPos(tri, nrm);
            }
        }
    }
    fillBuffer(mesh_buf_.vao, mesh_buf_.vbo, mesh_buf_.count, tri, true);

    // Unique edges as line segments (from the topology tables).
    const edit::Topology& topo = edit_.topology();
    std::vector<float> wire;
    wire.reserve(topo.edgeCount() * 6);
    for (size_t e = 0; e < topo.edgeCount(); ++e) {
        pushPos(wire, mesh_.positions[topo.edgeA[e]]);
        pushPos(wire, mesh_.positions[topo.edgeB[e]]);
    }
    fillBuffer(wire_.vao, wire_.vbo, wire_.count, wire, false);

    // Every vertex as a point (shown in vertex edit mode).
    std::vector<float> pts;
    pts.reserve(mesh_.vertexCount() * 3);
    for (const Vec3f& p : mesh_.positions) pushPos(pts, p);
    fillBuffer(allPoints_.vao, allPoints_.vbo, allPoints_.count, pts, false);
}

void Renderer::buildSelectionBuffers() {
    const edit::Topology& topo = edit_.topology();
    std::vector<float> faces, edges, pts;
    const edit::EditSession::Elem el = edit_.elem();
    if (el == edit::EditSession::Face) {
        std::vector<uint8_t> edgeMark(topo.edgeCount(), 0);
        for (size_t f = 0; f < mesh_.faceCount(); ++f) {
            if (!edit_.isSelected(edit::EditSession::Face, f)) continue;
            const uint32_t s = mesh_.faceStart[f], e = mesh_.faceStart[f + 1];
            for (uint32_t k = s + 1; k + 1 < e; ++k) {
                pushPos(faces, mesh_.positions[mesh_.faceVerts[s]]);
                pushPos(faces, mesh_.positions[mesh_.faceVerts[k]]);
                pushPos(faces, mesh_.positions[mesh_.faceVerts[k + 1]]);
            }
            for (uint32_t k = s; k < e; ++k) edgeMark[topo.faceEdges[k]] = 1;
        }
        for (size_t e = 0; e < edgeMark.size(); ++e)
            if (edgeMark[e]) {
                pushPos(edges, mesh_.positions[topo.edgeA[e]]);
                pushPos(edges, mesh_.positions[topo.edgeB[e]]);
            }
    } else if (el == edit::EditSession::Edge) {
        for (size_t e = 0; e < topo.edgeCount(); ++e)
            if (edit_.isSelected(edit::EditSession::Edge, e)) {
                pushPos(edges, mesh_.positions[topo.edgeA[e]]);
                pushPos(edges, mesh_.positions[topo.edgeB[e]]);
            }
    } else {
        for (size_t v = 0; v < mesh_.vertexCount(); ++v)
            if (edit_.isSelected(edit::EditSession::Vertex, v)) pushPos(pts, mesh_.positions[v]);
    }
    fillBuffer(selFaces_.vao, selFaces_.vbo, selFaces_.count, faces, false);
    fillBuffer(selEdges_.vao, selEdges_.vbo, selEdges_.count, edges, false);
    fillBuffer(selPoints_.vao, selPoints_.vbo, selPoints_.count, pts, false);
}

void Renderer::buildGrid() {
    const int N = 10;
    const float ext = N * gridStep_;
    std::vector<float> v;
    for (int i = -N; i <= N; ++i) {
        if (i == 0) continue;  // the axes are drawn separately
        const float t = i * gridStep_;
        v.insert(v.end(), {-ext, 0.0f, t, ext, 0.0f, t});
        v.insert(v.end(), {t, 0.0f, -ext, t, 0.0f, ext});
    }
    gridVerts_ = static_cast<int>(v.size() / 3);
    v.insert(v.end(), {-ext, 0.0f, 0.0f, ext, 0.0f, 0.0f});  // X axis
    v.insert(v.end(), {0.0f, 0.0f, 0.0f, 0.0f, ext, 0.0f});  // Y axis
    v.insert(v.end(), {0.0f, 0.0f, -ext, 0.0f, 0.0f, ext});  // Z axis
    int count = 0;
    fillBuffer(grid_.vao, grid_.vbo, count, v, false);
    grid_.count = count;
}

void Renderer::rebuildIfDirty() {
    if (!glReady_) return;
    const uint64_t geo = edit_.geoVersion(), sel = edit_.selVersion();
    if (meshDirty_ || geo != builtGeo_) {
        uploadMesh();
        builtGeo_ = geo;
        builtSel_ = ~0ull;  // selection overlays depend on positions
        meshDirty_ = false;
    }
    if (sel != builtSel_) {
        buildSelectionBuffers();
        builtSel_ = sel;
    }
}

// ================================================================================== drawing

namespace {
float ndcX(float px, int w) { return px / static_cast<float>(w) * 2.0f - 1.0f; }
float ndcY(float py, int h) { return 1.0f - py / static_cast<float>(h) * 2.0f; }
}  // namespace

void Renderer::drawGizmo(const edit::Camera& cam, const float* mvp) {
    const edit::EditSession::Gizmo g = edit_.gizmo(gizmoLen());
    if (!g.valid) return;
    edit::Vec2 p;
    if (!cam.project(g.pivot, p)) return;

    static const float kCol[3][4] = {{0.95f, 0.25f, 0.25f, 1.0f}, {0.35f, 0.85f, 0.35f, 1.0f}, {0.30f, 0.50f, 1.0f, 1.0f}};
    static const float kActive[4] = {1.0f, 0.9f, 0.2f, 1.0f};

    glDisable(GL_DEPTH_TEST);

    // Axis lines: five parallel 1px lines in screen space look like a thick line.
    std::vector<float> lines;
    int start[3] = {0, 0, 0}, count[3] = {0, 0, 0};
    for (int a = 0; a < 3; ++a) {
        edit::Vec2 t;
        if (!cam.project(g.tip[a], t)) continue;
        const float dx = t.x - p.x, dy = t.y - p.y;
        const float l = std::sqrt(dx * dx + dy * dy);
        if (l < 1.0f) continue;
        const float px = -dy / l, py = dx / l;
        start[a] = static_cast<int>(lines.size() / 3);
        for (int k = -2; k <= 2; ++k) {
            const float off = k * 0.8f * density_;
            lines.insert(lines.end(), {ndcX(p.x + px * off, width_), ndcY(p.y + py * off, height_), 0.0f,
                                       ndcX(t.x + px * off, width_), ndcY(t.y + py * off, height_), 0.0f});
        }
        count[a] = 10;
    }
    int total = 0;
    fillBuffer(gizmoLines_.vao, gizmoLines_.vbo, total, lines, false, GL_STREAM_DRAW);
    glUseProgram(lineProg_);
    glUniformMatrix4fv(uLineMvp_, 1, GL_FALSE, kIdentity);
    glBindVertexArray(gizmoLines_.vao);
    for (int a = 0; a < 3; ++a) {
        if (!count[a]) continue;
        const float* c = (touch_ == Touch::Gizmo && activeAxis_ == a) ? kActive : kCol[a];
        glUniform4f(uLineColor_, c[0], c[1], c[2], c[3]);
        glDrawArrays(GL_LINES, start[a], count[a]);
    }

    // Handles (round tips) at the end of each axis.
    std::vector<float> tips;
    for (int a = 0; a < 3; ++a) pushPos(tips, g.tip[a]);
    fillBuffer(gizmoTips_.vao, gizmoTips_.vbo, total, tips, false, GL_STREAM_DRAW);
    glUseProgram(pointProg_);
    glUniformMatrix4fv(uPointMvp_, 1, GL_FALSE, mvp);
    glUniform1f(uPointSize_, 16.0f * density_);
    glBindVertexArray(gizmoTips_.vao);
    for (int a = 0; a < 3; ++a) {
        const float* c = (touch_ == Touch::Gizmo && activeAxis_ == a) ? kActive : kCol[a];
        glUniform4f(uPointColor_, c[0], c[1], c[2], 1.0f);
        glDrawArrays(GL_POINTS, a, 1);
    }
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
}

void Renderer::drawRegion() {
    if (touch_ != Touch::Region || region_pts_.size() < 2) return;
    std::vector<float> v;
    for (const edit::Vec2& p : region_pts_) v.insert(v.end(), {ndcX(p.x, width_), ndcY(p.y, height_), 0.0f});
    int count = 0;
    fillBuffer(region_.vao, region_.vbo, count, v, false, GL_STREAM_DRAW);
    glDisable(GL_DEPTH_TEST);
    glUseProgram(lineProg_);
    glUniformMatrix4fv(uLineMvp_, 1, GL_FALSE, kIdentity);
    glUniform4f(uLineColor_, 0.55f, 0.80f, 1.0f, 1.0f);
    glBindVertexArray(region_.vao);
    glDrawArrays(GL_LINE_LOOP, 0, count);
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
}

void Renderer::onDrawFrame() {
    glClearColor(0.10f, 0.10f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!meshProg_ || !lineProg_ || !pointProg_) return;
    rebuildIfDirty();

    float mvp[16];
    Vec3f eye;
    viewProj(mvp, eye);
    const edit::Camera cam = makeCamera();
    const Vec3f tgt = {target_[0], target_[1], target_[2]};

    // Grid + axes.
    glUseProgram(lineProg_);
    glUniformMatrix4fv(uLineMvp_, 1, GL_FALSE, mvp);
    glBindVertexArray(grid_.vao);
    glUniform4f(uLineColor_, 0.22f, 0.22f, 0.26f, 1.0f);
    glDrawArrays(GL_LINES, 0, gridVerts_);
    glUniform4f(uLineColor_, 0.75f, 0.28f, 0.28f, 1.0f);  // X red
    glDrawArrays(GL_LINES, gridVerts_, 2);
    glUniform4f(uLineColor_, 0.40f, 0.70f, 0.35f, 1.0f);  // Y green
    glDrawArrays(GL_LINES, gridVerts_ + 2, 2);
    glUniform4f(uLineColor_, 0.30f, 0.45f, 0.80f, 1.0f);  // Z blue
    glDrawArrays(GL_LINES, gridVerts_ + 4, 2);

    // Filled mesh lit from the camera side, pushed back slightly so edges drawn on top do not flicker.
    Vec3f light = normalize(sub(eye, tgt));
    light = normalize(add(light, {0.0f, 0.5f, 0.0f}));
    glUseProgram(meshProg_);
    glUniformMatrix4fv(uMeshMvp_, 1, GL_FALSE, mvp);
    glUniform3f(uMeshLight_, light.x, light.y, light.z);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);
    glBindVertexArray(mesh_buf_.vao);
    glDrawArrays(GL_TRIANGLES, 0, mesh_buf_.count);
    glDisable(GL_POLYGON_OFFSET_FILL);

    // Edges.
    glUseProgram(lineProg_);
    glUniformMatrix4fv(uLineMvp_, 1, GL_FALSE, mvp);
    glUniform4f(uLineColor_, 0.05f, 0.05f, 0.07f, 1.0f);
    glBindVertexArray(wire_.vao);
    glDrawArrays(GL_LINES, 0, wire_.count);

    if (editMode_) {
        const float orange[4] = {1.0f, 0.60f, 0.10f, 1.0f};
        // Selected faces: translucent fill.
        if (selFaces_.count > 0) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(-1.0f, -1.0f);
            glUniform4f(uLineColor_, orange[0], orange[1], orange[2], 0.45f);
            glBindVertexArray(selFaces_.vao);
            glDrawArrays(GL_TRIANGLES, 0, selFaces_.count);
            glDisable(GL_POLYGON_OFFSET_FILL);
            glDisable(GL_BLEND);
        }
        // Selected edges / outlines.
        if (selEdges_.count > 0) {
            glUniform4f(uLineColor_, orange[0], orange[1], orange[2], 1.0f);
            glBindVertexArray(selEdges_.vao);
            glDrawArrays(GL_LINES, 0, selEdges_.count);
        }
        // Vertices.
        if (edit_.elem() == edit::EditSession::Vertex) {
            glUseProgram(pointProg_);
            glUniformMatrix4fv(uPointMvp_, 1, GL_FALSE, mvp);
            glUniform1f(uPointSize_, 3.2f * density_);
            glUniform4f(uPointColor_, 0.85f, 0.88f, 0.95f, 1.0f);
            glBindVertexArray(allPoints_.vao);
            glDrawArrays(GL_POINTS, 0, allPoints_.count);
            if (selPoints_.count > 0) {
                glUniform1f(uPointSize_, 5.5f * density_);
                glUniform4f(uPointColor_, orange[0], orange[1], orange[2], 1.0f);
                glBindVertexArray(selPoints_.vao);
                glDrawArrays(GL_POINTS, 0, selPoints_.count);
            }
        }
        glBindVertexArray(0);
        drawGizmo(cam, mvp);
        drawRegion();
    }
    glBindVertexArray(0);
}
