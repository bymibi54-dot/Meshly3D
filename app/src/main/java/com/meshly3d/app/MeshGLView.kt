package com.meshly3d.app

import android.content.Context
import android.opengl.GLSurfaceView
import android.view.MotionEvent
import android.view.ScaleGestureDetector

/** OpenGL ES 3 view: one finger orbits, two fingers pinch to zoom. */
class MeshGLView(context: Context) : GLSurfaceView(context) {

    private var lastX = 0f
    private var lastY = 0f

    private val scaleDetector = ScaleGestureDetector(
        context,
        object : ScaleGestureDetector.SimpleOnScaleGestureListener() {
            override fun onScale(detector: ScaleGestureDetector): Boolean {
                if (NativeBridge.ok) {
                    val s = detector.scaleFactor
                    queueEvent { NativeBridge.zoom(s) }
                    requestRender()
                }
                return true
            }
        }
    )

    init {
        setEGLContextClientVersion(3)
    }

    override fun onTouchEvent(e: MotionEvent): Boolean {
        scaleDetector.onTouchEvent(e)
        when (e.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                lastX = e.x
                lastY = e.y
            }
            MotionEvent.ACTION_POINTER_DOWN -> {
                lastX = e.x
                lastY = e.y
            }
            MotionEvent.ACTION_POINTER_UP -> {
                val keep = if (e.actionIndex == 0) 1 else 0
                lastX = e.getX(keep)
                lastY = e.getY(keep)
            }
            MotionEvent.ACTION_MOVE -> {
                if (e.pointerCount == 1 && !scaleDetector.isInProgress) {
                    val dx = e.x - lastX
                    val dy = e.y - lastY
                    lastX = e.x
                    lastY = e.y
                    if (NativeBridge.ok) {
                        queueEvent { NativeBridge.orbit(dx, dy) }
                        requestRender()
                    }
                }
            }
        }
        return true
    }
}
