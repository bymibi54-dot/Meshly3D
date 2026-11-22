package com.meshly3d.app

/** JNI bridge to the C++ core. All calls (except `ok`/`loadError`) must happen on the GL thread. */
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

    // camera
    external fun orbit(dx: Float, dy: Float)
    external fun pan(dx: Float, dy: Float)
    external fun zoom(scale: Float)
    external fun frame()

    // model
    external fun resetCube()

    /** Returns "" on success, otherwise an error message. */
    external fun loadGlb(data: ByteArray): String

    // editing
    external fun setEditMode(on: Boolean)
    external fun setElement(e: Int)   // 0 vertex, 1 edge, 2 face
    external fun setMethod(m: Int)    // 0 tap, 1 box, 2 lasso
    external fun setExtend(on: Boolean)
    external fun setTool(t: Int)      // 0 move, 1 rotate, 2 scale
    external fun undo()
    external fun redo()
    external fun selectAll()
    external fun selectNone()
    external fun setDensity(d: Float)

    // one-finger touches
    external fun touchDown(x: Float, y: Float)
    external fun touchMove(x: Float, y: Float)
    external fun touchUp(x: Float, y: Float)
    external fun touchCancel()

    external fun stats(): String
    external fun glInfo(): String
}
