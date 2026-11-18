package com.meshly3d.app

/** JNI bridge to the C++ core. All calls must happen on the GL thread. */
object NativeBridge {
    /** null = native library loaded fine; otherwise the reason it failed. */
    var loadError: String? = null
        private set

    val ok: Boolean get() = loadError == null

    init {
        try {
            System.loadLibrary("meshly_native")
        } catch (t: Throwable) {
            loadError = t.toString()
        }
    }

    external fun onSurfaceCreated()
    external fun onSurfaceChanged(width: Int, height: Int)
    external fun onDrawFrame()
    external fun orbit(dx: Float, dy: Float)
    external fun zoom(scale: Float)
    external fun glInfo(): String
}
