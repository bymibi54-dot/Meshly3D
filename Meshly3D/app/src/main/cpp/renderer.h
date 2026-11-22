#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

#include "mesh.h"

/** M1 renderer: OpenGL ES 3.x. Draws the mesh (flat shaded + edges), a grid with axes, orbit/pan/zoom camera. */
class Renderer {
public:
    static constexpr size_t kDefaultMaxTriangles = 700000;  // same default as the Design Doc

    Renderer();

    void onSurfaceCreated();
    void onSurfaceChanged(int width, int height);
    void onDrawFrame();

    void orbit(float dx, float dy);
    void pan(float dx, float dy);
    void zoom(float scale);
    void resetToCube();
    void frame();

    /** Returns "" on success, otherwise an error message. Must run on the GL thread. */
    std::string loadGlb(const uint8_t* data, size_t size);

    std::string stats() const;
    const std::string& info() const { return info_; }

private:
    void computeFrame();
    void uploadMesh();
    void buildGrid();

    Mesh mesh_;
    bool glReady_ = false;

    unsigned int meshProg_ = 0, lineProg_ = 0;
    int uMeshMvp_ = -1, uLineMvp_ = -1, uLineColor_ = -1;
    unsigned int meshVao_ = 0, meshVbo_ = 0, wireVao_ = 0, wireVbo_ = 0, gridVao_ = 0, gridVbo_ = 0;
    int meshVerts_ = 0, wireVerts_ = 0, gridVerts_ = 0;
    float gridStep_ = 1.0f;

    int width_ = 1, height_ = 1;
    float yaw_ = 35.0f, pitch_ = 25.0f;  // degrees
    float dist_ = 3.0f, radius_ = 1.0f;
    float target_[3] = {0.0f, 0.0f, 0.0f};
    float center_[3] = {0.0f, 0.0f, 0.0f};

    std::string info_;
};
