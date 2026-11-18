package com.meshly3d.app

import android.app.Activity
import android.app.ActivityManager
import android.opengl.GLSurfaceView
import android.os.Build
import android.os.Bundle
import android.view.Gravity
import android.view.View
import android.view.ViewGroup.LayoutParams.MATCH_PARENT
import android.view.ViewGroup.LayoutParams.WRAP_CONTENT
import android.widget.FrameLayout
import android.widget.ScrollView
import android.widget.TextView
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

class MainActivity : Activity() {

    private lateinit var glView: MeshGLView
    private lateinit var infoText: TextView
    private lateinit var reportScroll: ScrollView
    private lateinit var reportText: TextView
    private lateinit var deviceLine: String

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        CrashHandler.install(this)

        deviceLine = "Meshly3D diag · Android ${Build.VERSION.RELEASE} · ABI ${Build.SUPPORTED_ABIS.joinToString()}"

        glView = MeshGLView(this)
        infoText = TextView(this).apply {
            setTextColor(0xFFB8BCC8.toInt())
            textSize = 12f
            setPadding(24, 24, 24, 24)
            text = deviceLine
        }
        reportText = TextView(this).apply {
            setTextColor(0xFFFFB4A8.toInt())
            textSize = 11f
            setPadding(24, 48, 24, 24)
            setTextIsSelectable(true)
        }
        reportText.setOnClickListener {
            CrashHandler.clear(this)
            reportScroll.visibility = View.GONE
        }
        reportScroll = ScrollView(this).apply {
            setBackgroundColor(0xE6000000.toInt())
            visibility = View.GONE
            addView(reportText)
        }

        val root = FrameLayout(this)
        root.addView(glView, FrameLayout.LayoutParams(MATCH_PARENT, MATCH_PARENT))
        root.addView(
            infoText,
            FrameLayout.LayoutParams(WRAP_CONTENT, WRAP_CONTENT, Gravity.TOP or Gravity.START)
        )
        root.addView(reportScroll, FrameLayout.LayoutParams(MATCH_PARENT, MATCH_PARENT))
        setContentView(root)

        val report = buildReport()
        if (report.isNotEmpty()) {
            reportText.text = report + "\n(tap to close)"
            reportScroll.visibility = View.VISIBLE
        }

        glView.setRenderer(object : GLSurfaceView.Renderer {
            private var firstFrameDone = false
            private var glInfo = ""

            override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {
                if (!NativeBridge.ok) return
                NativeBridge.onSurfaceCreated()
                glInfo = NativeBridge.glInfo()
                this@MainActivity.runOnUiThread { infoText.text = deviceLine + "\n" + glInfo }
            }

            override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) {
                if (NativeBridge.ok) NativeBridge.onSurfaceChanged(width, height)
            }

            override fun onDrawFrame(gl: GL10?) {
                if (!NativeBridge.ok) return
                NativeBridge.onDrawFrame()
                if (!firstFrameDone) {
                    firstFrameDone = true
                    this@MainActivity.runOnUiThread {
                        infoText.text = deviceLine + "\n" + glInfo + "\nfirst frame OK"
                    }
                }
            }
        })
        // Draw only when something changes (no animation, saves battery).
        glView.renderMode = GLSurfaceView.RENDERMODE_WHEN_DIRTY
    }

    /** Everything useful to understand why the previous run died. Empty = nothing to report. */
    private fun buildReport(): String {
        val sb = StringBuilder()
        CrashHandler.read(this)?.let { sb.append("Last Java crash:\n").append(it).append("\n\n") }
        if (Build.VERSION.SDK_INT >= 30) {
            try {
                val am = getSystemService(ACTIVITY_SERVICE) as ActivityManager
                for (e in am.getHistoricalProcessExitReasons(packageName, 0, 1)) {
                    // 2 signaled, 3 low memory, 4 crash, 5 native crash, 6 ANR, 7 init failure, 8-9 other problems
                    if (e.reason in 2..9) {
                        sb.append("Last exit: reason=").append(e.reason)
                            .append(" status=").append(e.status)
                            .append(" ").append(e.description ?: "").append("\n\n")
                    }
                }
            } catch (ignored: Throwable) {
            }
        }
        NativeBridge.loadError?.let { sb.append("Native library error:\n").append(it).append("\n") }
        return sb.toString()
    }

    override fun onPause() {
        super.onPause()
        glView.onPause()
    }

    override fun onResume() {
        super.onResume()
        glView.onResume()
    }
}
