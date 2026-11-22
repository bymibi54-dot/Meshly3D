package com.meshly3d.app

import android.content.Context
import android.opengl.GLSurfaceView
import android.view.MotionEvent
import android.view.ScaleGestureDetector

/**
 * OpenGL ES 3 view.
 * One finger: sent to the C++ core (orbit, tap-select, box/lasso, gizmo drag).
 * Two fingers: pinch to zoom, drag to pan.
 */
class MeshGLView(context: Context) : GLSurfaceView(context) {

    /** Called after every gesture ends (used to refresh the counters on screen). */
    var onGestureEnd: (() -> Unit)? = null

    private var multi = false      // true while a second finger has been down in this gesture
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
        if (!NativeBridge.ok) return true
        when (e.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                multi = false
                val x = e.x
                val y = e.y
                queueEvent { NativeBridge.touchDown(x, y) }
                requestRender()
            }
            MotionEvent.ACTION_POINTER_DOWN -> {
                if (!multi) queueEvent { NativeBridge.touchCancel() }   // a second finger cancels the one-finger action
                multi = true
                computeFocus(e, -1)
                lastX = focusX
                lastY = focusY
                requestRender()
            }
            MotionEvent.ACTION_POINTER_UP -> {
                computeFocus(e, e.actionIndex)
                lastX = focusX
                lastY = focusY
            }
            MotionEvent.ACTION_MOVE -> {
                if (!multi && e.pointerCount == 1) {
                    val x = e.x
                    val y = e.y
                    queueEvent { NativeBridge.touchMove(x, y) }
                    requestRender()
                } else if (e.pointerCount == 2) {
                    computeFocus(e, -1)
                    val dx = focusX - lastX
                    val dy = focusY - lastY
                    lastX = focusX
                    lastY = focusY
                    queueEvent { NativeBridge.pan(dx, dy) }
                    requestRender()
                }
            }
            MotionEvent.ACTION_UP -> {
                val x = e.x
                val y = e.y
                if (!multi) queueEvent { NativeBridge.touchUp(x, y) }
                multi = false
                requestRender()
                onGestureEnd?.invoke()
            }
            MotionEvent.ACTION_CANCEL -> {
                queueEvent { NativeBridge.touchCancel() }
                multi = false
                requestRender()
                onGestureEnd?.invoke()
            }
        }
        return true
    }
}
