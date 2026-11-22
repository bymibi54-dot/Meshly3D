package com.meshly3d.app

import android.app.Activity
import android.app.ActivityManager
import android.content.Intent
import android.net.Uri
import android.opengl.GLSurfaceView
import android.os.Build
import android.os.Bundle
import android.provider.OpenableColumns
import android.view.Gravity
import android.view.View
import android.view.ViewGroup.LayoutParams.MATCH_PARENT
import android.view.ViewGroup.LayoutParams.WRAP_CONTENT
import android.widget.Button
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

class MainActivity : Activity() {

    private lateinit var glView: MeshGLView
    private lateinit var infoText: TextView
    private lateinit var reportScroll: ScrollView
    private lateinit var reportText: TextView

    // Lines of the info overlay (top-left).
    private var headerLine = ""
    private var glLine = ""
    private var statsLine = ""
    private var messageLine = ""

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        CrashHandler.install(this)

        headerLine = "Meshly3D M1 · Android ${Build.VERSION.RELEASE} · ${Build.SUPPORTED_ABIS.firstOrNull() ?: "?"}"

        glView = MeshGLView(this)
        infoText = TextView(this).apply {
            setTextColor(0xFFB8BCC8.toInt())
            textSize = 12f
            setPadding(24, 24, 24, 24)
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

        // Bottom bar: Open GLB | Cube | Frame
        val bar = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            setBackgroundColor(0xFF1A1A1F.toInt())
            setPadding(8, 8, 8, 8)
        }
        fun addButton(textRes: Int, action: () -> Unit) {
            val b = Button(this)
            b.setText(textRes)
            b.setOnClickListener { action() }
            bar.addView(b, LinearLayout.LayoutParams(0, WRAP_CONTENT, 1f))
        }
        addButton(R.string.btn_open) { openGlbPicker() }
        addButton(R.string.btn_cube) {
            runOnGl { NativeBridge.resetCube() }
            showMessage(getString(R.string.msg_cube))
        }
        addButton(R.string.btn_frame) { runOnGl { NativeBridge.frame() } }

        val root = FrameLayout(this)
        root.addView(glView, FrameLayout.LayoutParams(MATCH_PARENT, MATCH_PARENT))
        root.addView(
            infoText,
            FrameLayout.LayoutParams(WRAP_CONTENT, WRAP_CONTENT, Gravity.TOP or Gravity.START)
        )
        root.addView(bar, FrameLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT, Gravity.BOTTOM))
        root.addView(reportScroll, FrameLayout.LayoutParams(MATCH_PARENT, MATCH_PARENT))
        setContentView(root)

        val report = buildReport()
        if (report.isNotEmpty()) {
            reportText.text = report + "\n(tap to close)"
            reportScroll.visibility = View.VISIBLE
        }
        renderInfo()

        glView.setRenderer(object : GLSurfaceView.Renderer {
            override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {
                if (!NativeBridge.ok) return
                NativeBridge.onSurfaceCreated()
                val gpu = NativeBridge.glInfo()
                val stats = NativeBridge.stats()
                this@MainActivity.runOnUiThread {
                    glLine = gpu
                    statsLine = stats
                    renderInfo()
                }
            }

            override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) {
                if (NativeBridge.ok) NativeBridge.onSurfaceChanged(width, height)
            }

            override fun onDrawFrame(gl: GL10?) {
                if (NativeBridge.ok) NativeBridge.onDrawFrame()
            }
        })
        // Draw only when something changes (no animation, saves battery).
        glView.renderMode = GLSurfaceView.RENDERMODE_WHEN_DIRTY
    }

    private fun renderInfo() {
        infoText.text = listOf(headerLine, glLine, statsLine, messageLine)
            .filter { it.isNotEmpty() }
            .joinToString("\n")
    }

    private fun showMessage(msg: String) {
        messageLine = msg
        renderInfo()
    }

    /** Runs [block] on the GL thread, redraws, then refreshes the vertex/face counts on screen. */
    private fun runOnGl(block: () -> Unit) {
        if (!NativeBridge.ok) return
        glView.queueEvent {
            block()
            val stats = NativeBridge.stats()
            this@MainActivity.runOnUiThread {
                statsLine = stats
                renderInfo()
            }
        }
        glView.requestRender()
    }

    // ------------------------------------------------------------------ GLB import

    @Suppress("DEPRECATION")
    private fun openGlbPicker() {
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "*/*"
        }
        startActivityForResult(intent, REQ_OPEN)
    }

    @Suppress("DEPRECATION")
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode == REQ_OPEN && resultCode == RESULT_OK) {
            val uri = data?.data
            if (uri != null) loadGlbFromUri(uri)
        }
    }

    private fun displayName(uri: Uri): String {
        val name: String? = try {
            contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)?.use { c ->
                if (c.moveToFirst()) c.getString(0) else null
            }
        } catch (t: Throwable) {
            null
        }
        return name ?: (uri.lastPathSegment ?: "model")
    }

    private fun loadGlbFromUri(uri: Uri) {
        if (!NativeBridge.ok) return
        showMessage(getString(R.string.msg_loading))
        val name = displayName(uri)
        Thread {
            val bytes: ByteArray? = try {
                contentResolver.openInputStream(uri)?.use { it.readBytes() }
            } catch (t: Throwable) {
                null
            }
            if (bytes == null) {
                runOnUiThread { showMessage(getString(R.string.msg_read_failed)) }
            } else {
                glView.queueEvent {
                    val err = try {
                        NativeBridge.loadGlb(bytes)
                    } catch (t: Throwable) {
                        t.toString()
                    }
                    val stats = NativeBridge.stats()
                    runOnUiThread {
                        statsLine = stats
                        messageLine = if (err.isEmpty()) name else "Error: $err"
                        renderInfo()
                    }
                }
                glView.requestRender()
            }
        }.start()
    }

    // ------------------------------------------------------------------ diagnostics

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

    companion object {
        private const val REQ_OPEN = 1001
    }
}
