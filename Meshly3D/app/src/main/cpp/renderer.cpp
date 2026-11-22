#include "renderer.h"

#include <GLES3/gl3.h>
#include <android/log.h>

#include <algorithm>
#include <cmath>
#include <sstream>
#include <vector>

#include "glb_loader.h"

#define LOG_TAG "Meshly3D"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

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
out vec4 outColor;
void main() {
    vec3 n = normalize(vNormal);
    vec3 l = normalize(vec3(0.4, 0.8, 0.5));
    float d = max(dot(n, l), 0.0);
    vec3 base = vec3(0.62, 0.66, 0.72);
    outColor = vec4(base * (0.28 + 0.72 * d), 1.0);
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

struct Vec3 {
    float x, y, z;
};
Vec3 sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
Vec3 normalize(Vec3 a) {
    float l = std::sqrt(dot(a, a));
    return l > 0.0f ? Vec3{a.x / l, a.y / l, a.z / l} : a;
}

// Column-major 4x4 matrices (OpenGL convention).
void perspective(float* m, float fovyRad, float aspect, float zn, float zf) {
    std::fill(m, m + 16, 0.0f);
    float f = 1.0f / std::tan(fovyRad * 0.5f);
    m[0] = f / aspect;
    m[5] = f;
    m[10] = (zf + zn) / (zn - zf);
    m[11] = -1.0f;
    m[14] = 2.0f * zf * zn / (zn - zf);
}

void lookAt(float* m, Vec3 eye, Vec3 center, Vec3 up) {
    Vec3 f = normalize(sub(center, eye));
    Vec3 s = normalize(cross(f, up));
    Vec3 u = cross(s, f);
    m[0] = s.x;  m[4] = s.y;  m[8] = s.z;   m[12] = -dot(s, eye);
    m[1] = u.x;  m[5] = u.y;  m[9] = u.z;   m[13] = -dot(u, eye);
    m[2] = -f.x; m[6] = -f.y; m[10] = -f.z; m[14] = dot(f, eye);
    m[3] = 0.0f; m[7] = 0.0f; m[11] = 0.0f; m[15] = 1.0f;
}

void multiply(float* r, const float* a, const float* b) {
    for (int c = 0; c < 4; ++c) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) sum += a[k * 4 + row] * b[c * 4 + k];
            r[c * 4 + row] = sum;
        }
    }
}

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

void deleteVaoVbo(unsigned int& vao, unsigned int& vbo) {
    if (vbo) glDeleteBuffers(1, &vbo);
    if (vao) glDeleteVertexArrays(1, &vao);
    vao = vbo = 0;
}

// Creates (or refills) a VAO+VBO holding tightly packed floats; attribute 0 = position, optional attribute 1 = normal.
void fillBuffer(unsigned int& vao, unsigned int& vbo, const std::vector<float>& data, bool withNormal) {
    if (!vao) glGenVertexArrays(1, &vao);
    if (!vbo) glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.size() * sizeof(float)), data.data(), GL_STATIC_DRAW);
    const GLsizei stride = (withNormal ? 6 : 3) * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(0));
    if (withNormal) {
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(3 * sizeof(float)));
    }
    glBindVertexArray(0);
}

}  // namespace

Renderer::Renderer() {
    mesh_ = Mesh::makeCube();
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

void Renderer::frame() {
    computeFrame();
    if (glReady_) buildGrid();
}

void Renderer::resetToCube() {
    mesh_ = Mesh::makeCube();
    computeFrame();
    if (glReady_) {
        uploadMesh();
        buildGrid();
    }
}

std::string Renderer::loadGlb(const uint8_t* data, size_t size) {
    Mesh m;
    const std::string err = ::loadGlb(data, size, kDefaultMaxTriangles, m);
    if (!err.empty()) return err;
    mesh_ = std::move(m);
    computeFrame();
    if (glReady_) {
        uploadMesh();
        buildGrid();
    }
    return "";
}

std::string Renderer::stats() const {
    std::ostringstream os;
    os << "V " << mesh_.vertexCount() << " | F " << mesh_.faceCount() << " | T " << mesh_.triangleCount();
    return os.str();
}

void Renderer::onSurfaceCreated() {
    // A new GL context was created: all old object names are invalid.
    meshVao_ = meshVbo_ = wireVao_ = wireVbo_ = gridVao_ = gridVbo_ = 0;
    meshProg_ = lineProg_ = 0;

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
    if (meshProg_) uMeshMvp_ = glGetUniformLocation(meshProg_, "uMvp");
    if (lineProg_) {
        uLineMvp_ = glGetUniformLocation(lineProg_, "uMvp");
        uLineColor_ = glGetUniformLocation(lineProg_, "uColor");
    }
    glEnable(GL_DEPTH_TEST);
    glReady_ = true;
    uploadMesh();
    buildGrid();
}

void Renderer::onSurfaceChanged(int width, int height) {
    width_ = std::max(width, 1);
    height_ = std::max(height, 1);
    glViewport(0, 0, width_, height_);
}

void Renderer::uploadMesh() {
    deleteVaoVbo(meshVao_, meshVbo_);
    deleteVaoVbo(wireVao_, wireVbo_);

    // Filled triangles with flat normals (faces are fanned into triangles).
    std::vector<float> tri;
    tri.reserve(mesh_.triangleCount() * 18);
    std::vector<uint64_t> edges;
    edges.reserve(mesh_.faceVerts.size());
    for (size_t f = 0; f < mesh_.faceCount(); ++f) {
        const uint32_t s = mesh_.faceStart[f], e = mesh_.faceStart[f + 1];
        const uint32_t n = e - s;
        if (n < 3) continue;
        const uint32_t* v = &mesh_.faceVerts[s];

        Vec3 nrm = {0, 0, 0};  // Newell's method: works for any planar polygon
        for (uint32_t k = 0; k < n; ++k) {
            const Vec3f& a = mesh_.positions[v[k]];
            const Vec3f& b = mesh_.positions[v[(k + 1) % n]];
            nrm.x += (a.y - b.y) * (a.z + b.z);
            nrm.y += (a.z - b.z) * (a.x + b.x);
            nrm.z += (a.x - b.x) * (a.y + b.y);
            uint32_t ia = v[k], ib = v[(k + 1) % n];
            if (ia > ib) std::swap(ia, ib);
            edges.push_back((static_cast<uint64_t>(ia) << 32) | ib);
        }
        nrm = normalize(nrm);
        for (uint32_t k = 1; k + 1 < n; ++k) {
            const uint32_t corner[3] = {v[0], v[k], v[k + 1]};
            for (uint32_t c : corner) {
                const Vec3f& p = mesh_.positions[c];
                tri.insert(tri.end(), {p.x, p.y, p.z, nrm.x, nrm.y, nrm.z});
            }
        }
    }
    meshVerts_ = static_cast<int>(tri.size() / 6);
    fillBuffer(meshVao_, meshVbo_, tri, true);

    // Unique edges as line segments.
    std::sort(edges.begin(), edges.end());
    edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
    std::vector<float> wire;
    wire.reserve(edges.size() * 6);
    for (uint64_t key : edges) {
        const Vec3f& a = mesh_.positions[static_cast<uint32_t>(key >> 32)];
        const Vec3f& b = mesh_.positions[static_cast<uint32_t>(key & 0xFFFFFFFFu)];
        wire.insert(wire.end(), {a.x, a.y, a.z, b.x, b.y, b.z});
    }
    wireVerts_ = static_cast<int>(wire.size() / 3);
    fillBuffer(wireVao_, wireVbo_, wire, false);
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
    fillBuffer(gridVao_, gridVbo_, v, false);
}

void Renderer::onDrawFrame() {
    glClearColor(0.10f, 0.10f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!meshProg_ || !lineProg_) return;

    const float yaw = yaw_ * kDeg2Rad;
    const float pitch = pitch_ * kDeg2Rad;
    const Vec3 tgt = {target_[0], target_[1], target_[2]};
    const Vec3 eye = {tgt.x + dist_ * std::cos(pitch) * std::sin(yaw), tgt.y + dist_ * std::sin(pitch),
                      tgt.z + dist_ * std::cos(pitch) * std::cos(yaw)};

    float proj[16], view[16], mvp[16];
    perspective(proj, kFovDeg * kDeg2Rad, static_cast<float>(width_) / static_cast<float>(height_),
                std::max(dist_ * 0.01f, 1e-5f), dist_ * 100.0f);
    lookAt(view, eye, tgt, {0, 1, 0});
    multiply(mvp, proj, view);

    // Grid + axes.
    glUseProgram(lineProg_);
    glUniformMatrix4fv(uLineMvp_, 1, GL_FALSE, mvp);
    glBindVertexArray(gridVao_);
    glUniform4f(uLineColor_, 0.22f, 0.22f, 0.26f, 1.0f);
    glDrawArrays(GL_LINES, 0, gridVerts_);
    glUniform4f(uLineColor_, 0.75f, 0.28f, 0.28f, 1.0f);  // X red
    glDrawArrays(GL_LINES, gridVerts_, 2);
    glUniform4f(uLineColor_, 0.40f, 0.70f, 0.35f, 1.0f);  // Y green
    glDrawArrays(GL_LINES, gridVerts_ + 2, 2);
    glUniform4f(uLineColor_, 0.30f, 0.45f, 0.80f, 1.0f);  // Z blue
    glDrawArrays(GL_LINES, gridVerts_ + 4, 2);

    // Filled mesh, pushed back slightly so the edges drawn on top do not flicker.
    glUseProgram(meshProg_);
    glUniformMatrix4fv(uMeshMvp_, 1, GL_FALSE, mvp);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);
    glBindVertexArray(meshVao_);
    glDrawArrays(GL_TRIANGLES, 0, meshVerts_);
    glDisable(GL_POLYGON_OFFSET_FILL);

    // Edges.
    glUseProgram(lineProg_);
    glUniformMatrix4fv(uLineMvp_, 1, GL_FALSE, mvp);
    glUniform4f(uLineColor_, 0.05f, 0.05f, 0.07f, 1.0f);
    glBindVertexArray(wireVao_);
    glDrawArrays(GL_LINES, 0, wireVerts_);
    glBindVertexArray(0);
}

void Renderer::orbit(float dx, float dy) {
    yaw_ -= dx * 0.4f;
    pitch_ = std::clamp(pitch_ + dy * 0.4f, -89.0f, 89.0f);
}

void Renderer::pan(float dx, float dy) {
    const float yaw = yaw_ * kDeg2Rad;
    const float pitch = pitch_ * kDeg2Rad;
    // Camera looks from eye toward the target: forward = -(direction target->eye).
    const Vec3 fwd = normalize({-std::cos(pitch) * std::sin(yaw), -std::sin(pitch), -std::cos(pitch) * std::cos(yaw)});
    const Vec3 right = normalize(cross(fwd, {0, 1, 0}));
    const Vec3 up = cross(right, fwd);
    const float perPixel = 2.0f * dist_ * std::tan(kFovDeg * kDeg2Rad * 0.5f) / static_cast<float>(height_);
    target_[0] += -right.x * dx * perPixel + up.x * dy * perPixel;
    target_[1] += -right.y * dx * perPixel + up.y * dy * perPixel;
    target_[2] += -right.z * dx * perPixel + up.z * dy * perPixel;
}

void Renderer::zoom(float scale) {
    if (!(scale > 0.0f)) return;
    dist_ = std::clamp(dist_ / scale, radius_ * 0.05f, radius_ * 50.0f);
}
