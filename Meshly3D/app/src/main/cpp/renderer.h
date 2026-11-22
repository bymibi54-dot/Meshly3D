#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "editmesh.h"
#include "mesh.h"

/**
 * M2 renderer + editor controller: OpenGL ES 3.x.
 * Draws mesh (flat shaded + edges), grid with axes, selection overlays and the transform gizmo.
 * Touch events are interpreted here: orbit, tap-select, box / lasso select, gizmo drag.
 */
class Renderer {
public:
    static constexpr size_t kDefaultMaxTriangles = 700000;  // same default as the Design Doc

    Renderer();

    void onSurfaceCreated();
    void onSurfaceChanged(int width, int height);
    void onDrawFrame();

    // camera
    void orbit(float dx, float dy);
    void pan(float dx, float dy);
    void zoom(float scale);
    void frame();

    // model
    void resetToCube();
    /** Returns "" on success, otherwise an error message. Must run on the GL thread. */
    std::string loadGlb(const uint8_t* data, size_t size);

    // editing
    void setEditMode(bool on);
    void setElement(int e);  // 0 vertex, 1 edge, 2 face
    void setMethod(int m);   // 0 tap, 1 box, 2 lasso
    void setExtend(bool on);
    void setTool(int t);     // 0 move, 1 rotate, 2 scale
    void undo();
    void redo();
    void selectAll();
    void selectNone();
    void setDensity(float d) { density_ = d; }

    // one-finger touches (pixels, origin top-left)
    void touchDown(float x, float y);
    void touchMove(float x, float y);
    void touchUp(float x, float y);
    void touchCancel();

    std::string stats() const;
    /** "V F E T Sel Undo" as plain numbers separated by spaces (for the UI panels). */
    std::string meshInfo() const;
    const std::string& info() const { return info_; }

private:
    struct Buf {
        unsigned int vao = 0, vbo = 0;
        int count = 0;
    };
    enum class Touch { None, Orbit, Pending, Gizmo, Region };

    void computeFrame();
    void viewProj(float* vp, Vec3f& eye) const;
    edit::Camera makeCamera() const;
    float gizmoLen() const { return dist_ * 0.2f; }
    void markAllDirty();
    void uploadMesh();
    void buildSelectionBuffers();
    void buildGrid();
    void rebuildIfDirty();
    void drawGizmo(const edit::Camera& cam, const float* mvp);
    void drawRegion();

    Mesh mesh_;
    edit::EditSession edit_;
    bool glReady_ = false;

    unsigned int meshProg_ = 0, lineProg_ = 0, pointProg_ = 0;
    int uMeshMvp_ = -1, uMeshLight_ = -1;
    int uLineMvp_ = -1, uLineColor_ = -1;
    int uPointMvp_ = -1, uPointColor_ = -1, uPointSize_ = -1;

    Buf mesh_buf_, wire_, grid_, allPoints_, selFaces_, selEdges_, selPoints_, gizmoLines_, gizmoTips_, region_;
    int gridVerts_ = 0;
    float gridStep_ = 1.0f;
    uint64_t builtSel_ = ~0ull, builtGeo_ = ~0ull;
    bool meshDirty_ = true;

    // camera
    int width_ = 1, height_ = 1;
    float yaw_ = 35.0f, pitch_ = 25.0f;  // degrees
    float dist_ = 3.0f, radius_ = 1.0f;
    float target_[3] = {0.0f, 0.0f, 0.0f};
    float center_[3] = {0.0f, 0.0f, 0.0f};

    // editor state
    bool editMode_ = false;
    int method_ = 0;
    bool extend_ = false;
    edit::EditSession::Tool tool_ = edit::EditSession::Move;
    Touch touch_ = Touch::None;
    float startX_ = 0, startY_ = 0, lastX_ = 0, lastY_ = 0;
    int activeAxis_ = -1;
    std::vector<edit::Vec2> region_pts_;
    float density_ = 2.6f;

    std::string info_;
};
