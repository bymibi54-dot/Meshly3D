package com.meshly3d.app

import android.content.Context
import android.opengl.GLSurfaceView
import android.view.MotionEvent
import android.view.ScaleGestureDetector

/** OpenGL ES 3 view. One finger: orbit. Two fingers: pinch to zoom, drag to pan. */
class MeshGLView(context: Context) : GLSurfaceView(context) {

    private var lastX = 0f
    private var lastY = 0f
    private var focusX = 0f
    private var focusY = 0f

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

    /** Average position of all pointers (optionally ignoring the one that is lifting). */
    private fun computeFocus(e: MotionEvent, skip: Int) {
        var sx = 0f
        var sy = 0f
        var n = 0
        for (i in 0 until e.pointerCount) {
            if (i == skip) continue
            sx += e.getX(i)
            sy += e.getY(i)
            n++
        }
        if (n > 0) {
            focusX = sx / n
            focusY = sy / n
        }
    }

    override fun onTouchEvent(e: MotionEvent): Boolean {
        scaleDetector.onTouchEvent(e)
        when (e.actionMasked) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                computeFocus(e, -1)
                lastX = focusX
                lastY = focusY
            }
            MotionEvent.ACTION_POINTER_UP -> {
                computeFocus(e, e.actionIndex)
                lastX = focusX
                lastY = focusY
            }
            MotionEvent.ACTION_MOVE -> {
                computeFocus(e, -1)
                val dx = focusX - lastX
                val dy = focusY - lastY
                lastX = focusX
                lastY = focusY
                if (NativeBridge.ok) {
                    if (e.pointerCount == 1 && !scaleDetector.isInProgress) {
                        queueEvent { NativeBridge.orbit(dx, dy) }
                        requestRender()
                    } else if (e.pointerCount == 2) {
                        queueEvent { NativeBridge.pan(dx, dy) }
                        requestRender()
                    }
                }
            }
        }
        return true
    }
}
