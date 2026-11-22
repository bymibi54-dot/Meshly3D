#include <jni.h>

#include <string>
#include <vector>

#include "renderer.h"

static Renderer g_renderer;

// JNI's NewStringUTF needs valid (modified) UTF-8: keep plain printable ASCII only.
static jstring toJString(JNIEnv* env, std::string s) {
    for (char& c : s)
        if (static_cast<unsigned char>(c) < 0x20 || static_cast<unsigned char>(c) > 0x7E) c = '?';
    return env->NewStringUTF(s.c_str());
}

extern "C" {

JNIEXPORT void JNICALL Java_com_meshly3d_app_NativeBridge_onSurfaceCreated(JNIEnv*, jobject) {
    g_renderer.onSurfaceCreated();
}

JNIEXPORT void JNICALL Java_com_meshly3d_app_NativeBridge_onSurfaceChanged(JNIEnv*, jobject, jint w, jint h) {
    g_renderer.onSurfaceChanged(w, h);
}

JNIEXPORT void JNICALL Java_com_meshly3d_app_NativeBridge_onDrawFrame(JNIEnv*, jobject) {
    g_renderer.onDrawFrame();
}

JNIEXPORT void JNICALL Java_com_meshly3d_app_NativeBridge_orbit(JNIEnv*, jobject, jfloat dx, jfloat dy) {
    g_renderer.orbit(dx, dy);
}

JNIEXPORT void JNICALL Java_com_meshly3d_app_NativeBridge_pan(JNIEnv*, jobject, jfloat dx, jfloat dy) {
    g_renderer.pan(dx, dy);
}

JNIEXPORT void JNICALL Java_com_meshly3d_app_NativeBridge_zoom(JNIEnv*, jobject, jfloat scale) {
    g_renderer.zoom(scale);
}

JNIEXPORT void JNICALL Java_com_meshly3d_app_NativeBridge_resetCube(JNIEnv*, jobject) {
    g_renderer.resetToCube();
}

JNIEXPORT void JNICALL Java_com_meshly3d_app_NativeBridge_frame(JNIEnv*, jobject) {
    g_renderer.frame();
}

JNIEXPORT jstring JNICALL Java_com_meshly3d_app_NativeBridge_loadGlb(JNIEnv* env, jobject, jbyteArray data) {
    if (!data) return toJString(env, "No data");
    const jsize n = env->GetArrayLength(data);
    std::vector<uint8_t> buf(static_cast<size_t>(n));
    if (n > 0) env->GetByteArrayRegion(data, 0, n, reinterpret_cast<jbyte*>(buf.data()));
    return toJString(env, g_renderer.loadGlb(buf.data(), buf.size()));
}

JNIEXPORT jstring JNICALL Java_com_meshly3d_app_NativeBridge_stats(JNIEnv* env, jobject) {
    return toJString(env, g_renderer.stats());
}

JNIEXPORT jstring JNICALL Java_com_meshly3d_app_NativeBridge_glInfo(JNIEnv* env, jobject) {
    return toJString(env, g_renderer.info());
}


JNIEXPORT void JNICALL Java_com_meshly3d_app_NativeBridge_setEditMode(JNIEnv*, jobject, jboolean on) {
    g_renderer.setEditMode(on != 0);
}
JNIEXPORT void JNICALL Java_com_meshly3d_app_NativeBridge_setElement(JNIEnv*, jobject, jint e) {
    g_renderer.setElement(e);
}
JNIEXPORT void JNICALL Java_com_meshly3d_app_NativeBridge_setMethod(JNIEnv*, jobject, jint m) {
    g_renderer.setMethod(m);
}
JNIEXPORT void JNICALL Java_com_meshly3d_app_NativeBridge_setExtend(JNIEnv*, jobject, jboolean on) {
    g_renderer.setExtend(on != 0);
}
JNIEXPORT void JNICALL Java_com_meshly3d_app_NativeBridge_setTool(JNIEnv*, jobject, jint t) {
    g_renderer.setTool(t);
}
JNIEXPORT void JNICALL Java_com_meshly3d_app_NativeBridge_undo(JNIEnv*, jobject) { g_renderer.undo(); }
JNIEXPORT void JNICALL Java_com_meshly3d_app_NativeBridge_redo(JNIEnv*, jobject) { g_renderer.redo(); }
JNIEXPORT void JNICALL Java_com_meshly3d_app_NativeBridge_selectAll(JNIEnv*, jobject) { g_renderer.selectAll(); }
JNIEXPORT void JNICALL Java_com_meshly3d_app_NativeBridge_selectNone(JNIEnv*, jobject) { g_renderer.selectNone(); }
JNIEXPORT void JNICALL Java_com_meshly3d_app_NativeBridge_setDensity(JNIEnv*, jobject, jfloat d) {
    g_renderer.setDensity(d);
}
JNIEXPORT void JNICALL Java_com_meshly3d_app_NativeBridge_touchDown(JNIEnv*, jobject, jfloat x, jfloat y) {
    g_renderer.touchDown(x, y);
}
JNIEXPORT void JNICALL Java_com_meshly3d_app_NativeBridge_touchMove(JNIEnv*, jobject, jfloat x, jfloat y) {
    g_renderer.touchMove(x, y);
}
JNIEXPORT void JNICALL Java_com_meshly3d_app_NativeBridge_touchUp(JNIEnv*, jobject, jfloat x, jfloat y) {
    g_renderer.touchUp(x, y);
}
JNIEXPORT void JNICALL Java_com_meshly3d_app_NativeBridge_touchCancel(JNIEnv*, jobject) { g_renderer.touchCancel(); }

}  // extern "C"
