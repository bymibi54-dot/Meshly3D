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
import android.widget.HorizontalScrollView
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
    private lateinit var editScroll: HorizontalScrollView

    private lateinit var modeChips: List<Button>
    private lateinit var elemChips: List<Button>
    private lateinit var methodChips: List<Button>
    private lateinit var toolChips: List<Button>
    private lateinit var extendChip: Button
    private var extendOn = false

    // Lines of the info overlay (top-left).
    private var headerLine = ""
    private var glLine = ""
    private var statsLine = ""
    private var messageLine = ""

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        CrashHandler.install(this)

        headerLine = "Meshly3D M2 · Android ${Build.VERSION.RELEASE} · ${Build.SUPPORTED_ABIS.firstOrNull() ?: "?"}"

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

        // ---------------------------------------------------------------- bottom bars
        // Row 1 (Edit mode only): what to select, how to select, which tool.
        val editRow = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL }
        val elemList = listOf(
            chip(R.string.elem_vertex) { setElement(0) },
            chip(R.string.elem_edge) { setElement(1) },
            chip(R.string.elem_face) { setElement(2) }
        )
        val methodList = listOf(
            chip(R.string.method_tap) { setMethod(0) },
            chip(R.string.method_box) { setMethod(1) },
            chip(R.string.method_lasso) { setMethod(2) }
        )
        extendChip = chip(R.string.sel_add) { toggleExtend() }
        val toolList = listOf(
            chip(R.string.tool_move) { setTool(0) },
            chip(R.string.tool_rotate) { setTool(1) },
            chip(R.string.tool_scale) { setTool(2) }
        )
        elemChips = elemList
        methodChips = methodList
        toolChips = toolList
        for (b in elemList) addChip(editRow, b)
        addSpacer(editRow)
        for (b in methodList) addChip(editRow, b)
        addChip(editRow, extendChip)
        addSpacer(editRow)
        for (b in toolList) addChip(editRow, b)
        addSpacer(editRow)
        addChip(editRow, chip(R.string.sel_all) { runOnGl { NativeBridge.selectAll() } })
        addChip(editRow, chip(R.string.sel_none) { runOnGl { NativeBridge.selectNone() } })
        editScroll = HorizontalScrollView(this).apply {
            isHorizontalScrollBarEnabled = false
            visibility = View.GONE
            addView(editRow)
        }

        // Row 2 (always): mode, undo / redo, file and camera actions.
        val actionRow = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL }
        val modeList = listOf(
            chip(R.string.mode_object) { setEditMode(false) },
            chip(R.string.mode_edit) { setEditMode(true) }
        )
        modeChips = modeList
        for (b in modeList) addChip(actionRow, b)
        addSpacer(actionRow)
        addChip(actionRow, chip(R.string.act_undo) { runOnGl { NativeBridge.undo() } })
        addChip(actionRow, chip(R.string.act_redo) { runOnGl { NativeBridge.redo() } })
        addSpacer(actionRow)
        addChip(actionRow, chip(R.string.btn_open) { openGlbPicker() })
        addChip(actionRow, chip(R.string.btn_cube) {
            runOnGl { NativeBridge.resetCube() }
            showMessage(getString(R.string.msg_cube))
        })
        addChip(actionRow, chip(R.string.btn_frame) { runOnGl { NativeBridge.frame() } })
        val actionScroll = HorizontalScrollView(this).apply {
            isHorizontalScrollBarEnabled = false
            addView(actionRow)
        }

        val bottom = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(0xFF1A1A1F.toInt())
            addView(editScroll, LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT))
            addView(actionScroll, LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT))
        }

        val root = FrameLayout(this)
        root.addView(glView, FrameLayout.LayoutParams(MATCH_PARENT, MATCH_PARENT))
        root.addView(
            infoText,
            FrameLayout.LayoutParams(WRAP_CONTENT, WRAP_CONTENT, Gravity.TOP or Gravity.START)
        )
        root.addView(bottom, FrameLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT, Gravity.BOTTOM))
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
        glView.onGestureEnd = { runOnGl { } }   // refresh the counters after every gesture

        // Initial state (Object mode, vertex select, tap, move tool).
        val density = resources.displayMetrics.density
        select(modeChips, 0)
        select(elemChips, 0)
        select(methodChips, 0)
        select(toolChips, 0)
        styleChip(extendChip, false)
        runOnGl {
            NativeBridge.setDensity(density)
            NativeBridge.setEditMode(false)
            NativeBridge.setElement(0)
            NativeBridge.setMethod(0)
            NativeBridge.setExtend(false)
            NativeBridge.setTool(0)
        }
    }

    // ------------------------------------------------------------------ small UI helpers

    private fun dp(v: Int): Int = (v * resources.displayMetrics.density + 0.5f).toInt()

    private fun styleChip(b: Button, active: Boolean) {
        b.setBackgroundColor(if (active) 0xFF3A6EA5.toInt() else 0xFF2A2A30.toInt())
    }

    private fun chip(textRes: Int, onClick: () -> Unit): Button {
        val b = Button(this)
        b.setText(textRes)
        b.isAllCaps = false
        b.textSize = 12f
        b.minWidth = 0
        b.minimumWidth = 0
        b.minHeight = 0
        b.minimumHeight = 0
        b.setPadding(dp(14), dp(6), dp(14), dp(6))
        b.stateListAnimator = null
        b.setTextColor(0xFFFFFFFF.toInt())
        b.setOnClickListener { onClick() }
        styleChip(b, false)
        return b
    }

    private fun addChip(row: LinearLayout, b: Button) {
        val lp = LinearLayout.LayoutParams(WRAP_CONTENT, dp(40))
        lp.setMargins(dp(3), dp(3), dp(3), dp(3))
        row.addView(b, lp)
    }

    private fun addSpacer(row: LinearLayout) {
        row.addView(View(this), LinearLayout.LayoutParams(dp(10), dp(1)))
    }

    private fun select(group: List<Button>, index: Int) {
        group.forEachIndexed { i, b -> styleChip(b, i == index) }
    }

    // ------------------------------------------------------------------ editing controls

    private fun setEditMode(on: Boolean) {
        editScroll.visibility = if (on) View.VISIBLE else View.GONE
        select(modeChips, if (on) 1 else 0)
        runOnGl { NativeBridge.setEditMode(on) }
    }

    private fun setElement(e: Int) {
        select(elemChips, e)
        runOnGl { NativeBridge.setElement(e) }
    }

    private fun setMethod(m: Int) {
        select(methodChips, m)
        runOnGl { NativeBridge.setMethod(m) }
    }

    private fun setTool(t: Int) {
        select(toolChips, t)
        runOnGl { NativeBridge.setTool(t) }
    }

    private fun toggleExtend() {
        extendOn = !extendOn
        val v = extendOn
        styleChip(extendChip, v)
        runOnGl { NativeBridge.setExtend(v) }
    }

    // ------------------------------------------------------------------ info overlay

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
