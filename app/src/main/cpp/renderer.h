#pragma once
#include <string>

/** M0 renderer: OpenGL ES 3.x, one shaded cube, orbit camera. */
class Renderer {
public:
    void onSurfaceCreated();
    void onSurfaceChanged(int width, int height);
    void onDrawFrame();
    void orbit(float dx, float dy);
    void zoom(float scale);
    const std::string& info() const { return info_; }

private:
    unsigned int prog_ = 0;
    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
    int uMvp_ = -1;
    int vertexCount_ = 0;
    int width_ = 1;
    int height_ = 1;
    float yaw_ = 35.0f;    // degrees
    float pitch_ = 25.0f;  // degrees
    float dist_ = 3.5f;
    std::string info_;
};
