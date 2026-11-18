#include "renderer.h"

#include <GLES3/gl3.h>
#include <android/log.h>

#include <algorithm>
#include <cmath>
#include <vector>

#define LOG_TAG "Meshly3D"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

const char* kVertexShader = R"(#version 300 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
uniform mat4 uMvp;
out vec3 vNormal;
void main() {
    vNormal = aNormal;
    gl_Position = uMvp * vec4(aPos, 1.0);
}
)";

const char* kFragmentShader = R"(#version 300 es
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

// Unit cube, 36 vertices, each = position (3) + normal (3). CCW winding.
void buildCube(std::vector<float>& out) {
    const float h = 0.5f;
    struct Face {
        float n[3], u[3], v[3];
    };
    const Face faces[6] = {
        {{1, 0, 0}, {0, 0, -1}, {0, 1, 0}},
        {{-1, 0, 0}, {0, 0, 1}, {0, 1, 0}},
        {{0, 1, 0}, {1, 0, 0}, {0, 0, -1}},
        {{0, -1, 0}, {1, 0, 0}, {0, 0, 1}},
        {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}},
        {{0, 0, -1}, {-1, 0, 0}, {0, 1, 0}},
    };
    const float cs[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
    const int tri[6] = {0, 1, 2, 0, 2, 3};
    for (const Face& f : faces) {
        for (int i = 0; i < 6; ++i) {
            const float su = cs[tri[i]][0];
            const float sv = cs[tri[i]][1];
            for (int a = 0; a < 3; ++a) out.push_back(f.n[a] * h + f.u[a] * su * h + f.v[a] * sv * h);
            for (int a = 0; a < 3; ++a) out.push_back(f.n[a]);
        }
    }
}

}  // namespace

void Renderer::onSurfaceCreated() {
    // A new GL context was created: old object names are invalid.
    prog_ = vao_ = vbo_ = 0;

    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    info_ = std::string(version ? version : "?") + "\n" + (renderer ? renderer : "?");
    LOGI("GL_VERSION: %s", version ? version : "?");
    LOGI("GL_RENDERER: %s", renderer ? renderer : "?");

    prog_ = link(kVertexShader, kFragmentShader);
    if (!prog_) return;
    uMvp_ = glGetUniformLocation(prog_, "uMvp");

    std::vector<float> data;
    buildCube(data);
    vertexCount_ = static_cast<int>(data.size() / 6);

    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);
    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.size() * sizeof(float)), data.data(),
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          reinterpret_cast<void*>(3 * sizeof(float)));
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
}

void Renderer::onSurfaceChanged(int width, int height) {
    width_ = std::max(width, 1);
    height_ = std::max(height, 1);
    glViewport(0, 0, width_, height_);
}

void Renderer::onDrawFrame() {
    glClearColor(0.10f, 0.10f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!prog_) return;

    const float kDeg2Rad = 3.14159265358979f / 180.0f;
    const float yaw = yaw_ * kDeg2Rad;
    const float pitch = pitch_ * kDeg2Rad;
    Vec3 eye = {dist_ * std::cos(pitch) * std::sin(yaw), dist_ * std::sin(pitch),
                dist_ * std::cos(pitch) * std::cos(yaw)};

    float proj[16], view[16], mvp[16];
    perspective(proj, 50.0f * kDeg2Rad, static_cast<float>(width_) / static_cast<float>(height_), 0.1f,
                100.0f);
    lookAt(view, eye, {0, 0, 0}, {0, 1, 0});
    multiply(mvp, proj, view);

    glUseProgram(prog_);
    glUniformMatrix4fv(uMvp_, 1, GL_FALSE, mvp);
    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, vertexCount_);
    glBindVertexArray(0);
}

void Renderer::orbit(float dx, float dy) {
    yaw_ -= dx * 0.4f;
    pitch_ = std::clamp(pitch_ + dy * 0.4f, -89.0f, 89.0f);
}

void Renderer::zoom(float scale) {
    if (scale <= 0.0f) return;
    dist_ = std::clamp(dist_ / scale, 1.5f, 20.0f);
}
