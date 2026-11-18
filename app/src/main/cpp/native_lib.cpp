#include <jni.h>

#include "renderer.h"

static Renderer g_renderer;

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

JNIEXPORT void JNICALL Java_com_meshly3d_app_NativeBridge_zoom(JNIEnv*, jobject, jfloat scale) {
    g_renderer.zoom(scale);
}

JNIEXPORT jstring JNICALL Java_com_meshly3d_app_NativeBridge_glInfo(JNIEnv* env, jobject) {
    return env->NewStringUTF(g_renderer.info().c_str());
}

}  // extern "C"
