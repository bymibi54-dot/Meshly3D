package com.meshly3d.app

import android.content.Context
import android.graphics.Canvas
import android.graphics.DashPathEffect
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RectF
import android.view.View
import kotlin.math.cos
import kotlin.math.min
import kotlin.math.sin

/** Small line icons for the left toolbar, drawn with Canvas (no image files). */
class ToolIcon(context: Context, private val kind: Int) : View(context) {

    var tint: Int = 0xFFFFFFFF.toInt()
        set(value) {
            field = value
            invalidate()
        }

    private val p = Paint(Paint.ANTI_ALIAS_FLAG)
    private val path = Path()

    override fun onDraw(c: Canvas) {
        val cx = width / 2f
        val cy = height / 2f
        val r = min(width, height) * 0.42f
        p.color = tint
        p.strokeCap = Paint.Cap.ROUND
        p.strokeJoin = Paint.Join.ROUND
        p.strokeWidth = maxOf(2f, r * 0.16f)
        p.style = Paint.Style.STROKE
        p.pathEffect = null

        when (kind) {
            SELECT -> {                       // pointer arrow
                val pts = floatArrayOf(-0.45f, -0.9f, -0.45f, 0.65f, -0.10f, 0.32f, 0.20f, 0.95f, 0.42f, 0.85f, 0.14f, 0.25f, 0.60f, 0.20f)
                polygon(cx, cy, r, pts)
                p.style = Paint.Style.FILL
                c.drawPath(path, p)
            }
            MOVE -> {                         // four arrows
                c.drawLine(cx - r, cy, cx + r, cy, p)
                c.drawLine(cx, cy - r, cx, cy + r, p)
                val a = r * 0.34f
                c.drawLine(cx + r, cy, cx + r - a, cy - a, p)
                c.drawLine(cx + r, cy, cx + r - a, cy + a, p)
                c.drawLine(cx - r, cy, cx - r + a, cy - a, p)
                c.drawLine(cx - r, cy, cx - r + a, cy + a, p)
                c.drawLine(cx, cy - r, cx - a, cy - r + a, p)
                c.drawLine(cx, cy - r, cx + a, cy - r + a, p)
                c.drawLine(cx, cy + r, cx - a, cy + r - a, p)
                c.drawLine(cx, cy + r, cx + a, cy + r - a, p)
            }
            ROTATE -> {                       // circular arrow
                val rr = r * 0.8f
                c.drawArc(RectF(cx - rr, cy - rr, cx + rr, cy + rr), 40f, 270f, false, p)
                val a = Math.toRadians(310.0)
                val ex = cx + rr * cos(a).toFloat()
                val ey = cy + rr * sin(a).toFloat()
                val tx = -sin(a).toFloat()
                val ty = cos(a).toFloat()
                val nx = cos(a).toFloat()
                val ny = sin(a).toFloat()
                path.reset()
                path.moveTo(ex + tx * r * 0.45f, ey + ty * r * 0.45f)
                path.lineTo(ex + nx * r * 0.32f, ey + ny * r * 0.32f)
                path.lineTo(ex - nx * r * 0.32f, ey - ny * r * 0.32f)
                path.close()
                p.style = Paint.Style.FILL
                c.drawPath(path, p)
            }
            SCALE -> {                        // frame with a diagonal arrow
                c.drawRect(cx - r * 0.8f, cy - r * 0.8f, cx + r * 0.8f, cy + r * 0.8f, p)
                c.drawLine(cx - r * 0.25f, cy + r * 0.25f, cx + r * 0.6f, cy - r * 0.6f, p)
                c.drawLine(cx + r * 0.6f, cy - r * 0.6f, cx + r * 0.15f, cy - r * 0.6f, p)
                c.drawLine(cx + r * 0.6f, cy - r * 0.6f, cx + r * 0.6f, cy - r * 0.15f, p)
            }
            VERTEX -> {
                p.style = Paint.Style.FILL
                c.drawCircle(cx, cy, r * 0.45f, p)
            }
            EDGE -> {
                c.drawLine(cx - r * 0.8f, cy + r * 0.8f, cx + r * 0.8f, cy - r * 0.8f, p)
                p.style = Paint.Style.FILL
                c.drawCircle(cx - r * 0.8f, cy + r * 0.8f, r * 0.24f, p)
                c.drawCircle(cx + r * 0.8f, cy - r * 0.8f, r * 0.24f, p)
            }
            FACE -> {
                polygon(cx, cy, r, floatArrayOf(-0.9f, 0.55f, -0.45f, -0.75f, 0.9f, -0.55f, 0.55f, 0.75f))
                c.drawPath(path, p)
                p.style = Paint.Style.FILL
                p.alpha = 90
                c.drawPath(path, p)
            }
            BOX -> {                          // dashed rectangle
                p.pathEffect = DashPathEffect(floatArrayOf(r * 0.35f, r * 0.25f), 0f)
                c.drawRect(cx - r * 0.85f, cy - r * 0.65f, cx + r * 0.85f, cy + r * 0.65f, p)
            }
            LASSO -> {                        // loop with a tail
                path.reset()
                path.moveTo(cx - r * 0.8f, cy - r * 0.1f)
                path.cubicTo(cx - r * 0.8f, cy - r * 0.95f, cx + r * 0.8f, cy - r * 0.95f, cx + r * 0.8f, cy - r * 0.1f)
                path.cubicTo(cx + r * 0.8f, cy + r * 0.5f, cx - r * 0.3f, cy + r * 0.6f, cx - r * 0.4f, cy + r * 0.2f)
                path.lineTo(cx - r * 0.2f, cy + r * 0.9f)
                c.drawPath(path, p)
            }
            UNDO, REDO -> {                   // curved arrow (mirrored for redo)
                val d = if (kind == UNDO) 1f else -1f
                path.reset()
                path.moveTo(cx - d * r * 0.7f, cy - r * 0.2f)
                path.cubicTo(cx - d * r * 0.2f, cy - r * 0.95f, cx + d * r * 0.95f, cy - r * 0.5f, cx + d * r * 0.8f, cy + r * 0.45f)
                c.drawPath(path, p)
                c.drawLine(cx - d * r * 0.7f, cy - r * 0.2f, cx - d * r * 0.7f, cy - r * 0.75f, p)
                c.drawLine(cx - d * r * 0.7f, cy - r * 0.2f, cx - d * r * 0.15f, cy - r * 0.2f, p)
            }
            OPEN -> {                         // tray with an arrow going in
                path.reset()
                path.moveTo(cx - r * 0.85f, cy + r * 0.1f)
                path.lineTo(cx - r * 0.85f, cy + r * 0.8f)
                path.lineTo(cx + r * 0.85f, cy + r * 0.8f)
                path.lineTo(cx + r * 0.85f, cy + r * 0.1f)
                c.drawPath(path, p)
                c.drawLine(cx, cy - r * 0.85f, cx, cy + r * 0.45f, p)
                c.drawLine(cx, cy + r * 0.45f, cx - r * 0.35f, cy + r * 0.1f, p)
                c.drawLine(cx, cy + r * 0.45f, cx + r * 0.35f, cy + r * 0.1f, p)
            }
            FRAME -> {                        // four corner brackets
                val a = r * 0.9f
                val b = r * 0.4f
                for (sx in floatArrayOf(-1f, 1f))
                    for (sy in floatArrayOf(-1f, 1f)) {
                        c.drawLine(cx + sx * a, cy + sy * a, cx + sx * (a - b), cy + sy * a, p)
                        c.drawLine(cx + sx * a, cy + sy * a, cx + sx * a, cy + sy * (a - b), p)
                    }
            }
            ALL -> {
                p.style = Paint.Style.FILL
                c.drawRect(cx - r * 0.75f, cy - r * 0.75f, cx + r * 0.75f, cy + r * 0.75f, p)
            }
            NONE -> {
                c.drawRect(cx - r * 0.75f, cy - r * 0.75f, cx + r * 0.75f, cy + r * 0.75f, p)
            }
            ADD -> {                          // plus
                c.drawLine(cx - r * 0.8f, cy, cx + r * 0.8f, cy, p)
                c.drawLine(cx, cy - r * 0.8f, cx, cy + r * 0.8f, p)
            }
            CUBE -> {                         // isometric cube
                path.reset()
                for (k in 0 until 6) {
                    val a = Math.toRadians(-90.0 + 60.0 * k)
                    val x = cx + r * 0.95f * cos(a).toFloat()
                    val y = cy + r * 0.95f * sin(a).toFloat()
                    if (k == 0) path.moveTo(x, y) else path.lineTo(x, y)
                }
                path.close()
                c.drawPath(path, p)
                for (k in intArrayOf(1, 3, 5)) {
                    val a = Math.toRadians(-90.0 + 60.0 * k)
                    c.drawLine(cx, cy, cx + r * 0.95f * cos(a).toFloat(), cy + r * 0.95f * sin(a).toFloat(), p)
                }
            }
        }
    }

    private fun polygon(cx: Float, cy: Float, r: Float, pts: FloatArray) {
        path.reset()
        var i = 0
        while (i + 1 < pts.size) {
            val x = cx + pts[i] * r
            val y = cy + pts[i + 1] * r
            if (i == 0) path.moveTo(x, y) else path.lineTo(x, y)
            i += 2
        }
        path.close()
    }

    companion object {
        const val SELECT = 0
        const val MOVE = 1
        const val ROTATE = 2
        const val SCALE = 3
        const val VERTEX = 4
        const val EDGE = 5
        const val FACE = 6
        const val BOX = 7
        const val LASSO = 8
        const val CUBE = 9
        const val UNDO = 10
        const val REDO = 11
        const val OPEN = 12
        const val FRAME = 13
        const val ALL = 14
        const val NONE = 15
        const val ADD = 16
    }
}
