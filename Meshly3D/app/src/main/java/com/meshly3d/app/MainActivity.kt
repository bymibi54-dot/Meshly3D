package com.meshly3d.app

import android.app.Activity
import android.app.ActivityManager
import android.content.Intent
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.net.Uri
import android.opengl.GLSurfaceView
import android.os.Build
import android.os.Bundle
import android.provider.OpenableColumns
import android.util.TypedValue
import android.view.Gravity
import android.view.View
import android.view.ViewGroup.LayoutParams.MATCH_PARENT
import android.view.ViewGroup.LayoutParams.WRAP_CONTENT
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

// ---- Palette (see the UI spec)
private val C_BG = 0xFF1A1A1A.toInt()          // viewport
private val C_BAR = 0xFF242424.toInt()         // top bar
private val C_TOOLBAR = 0xFF1E1E1E.toInt()     // left toolbar
private val C_RIGHT = 0xFF222222.toInt()       // right panel
private val C_STATUS = 0xFF111111.toInt()      // status bar
private val C_DIVIDER = 0xFF333333.toInt()
private val C_ORANGE = 0xFFFF6A00.toInt()      // accent
private val C_WHITE = 0xFFFFFFFF.toInt()
private val C_SECONDARY = 0xFFB0B0B0.toInt()
private val C_PILL_OFF = 0xFFD0D0D0.toInt()
private val C_DARK_TEXT = 0xFF1A1A1A.toInt()

/**
 * Set to false to show ONLY the 10 tools of the spec in the left toolbar.
 * With true, the actions that already existed (Undo, Redo, Open GLB, Frame, All, None, Add) stay reachable
 * below the 10 tools; without them a GLB can not be opened and a move can not be undone.
 */
private const val SHOW_EXTRA_ACTIONS = true

class MainActivity : Activity() {

    private lateinit var glView: MeshGLView
    private lateinit var reportScroll: ScrollView
    private lateinit var reportText: TextView
    private lateinit var statusText: TextView
    private lateinit var valueV: TextView
    private lateinit var valueF: TextView
    private lateinit var valueE: TextView
    private lateinit var pillObject: TextView
    private lateinit var pillEdit: TextView

    private lateinit var tSelect: Tile
    private lateinit var tBox: Tile
    private lateinit var tLasso: Tile
    private lateinit var tMove: Tile
    private lateinit var tRotate: Tile
    private lateinit var tScale: Tile
    private lateinit var tVertex: Tile
    private lateinit var tEdge: Tile
    private lateinit var tFace: Tile
    private var tAdd: Tile? = null

    private val editOnlyTiles = mutableListOf<Tile>()
    private var editOn = false
    private var extendOn = false

    /** Layout numbers in the spec are pixels of a 1080-px-wide screen; scale them to this device. */
    private val uiScale: Float by lazy {
        val dm = resources.displayMetrics
        minOf(dm.widthPixels, dm.heightPixels) / 1080f
    }

    private fun px(v: Int): Int = maxOf(1, (v * uiScale + 0.5f).toInt())

    private val bold: Typeface = Typeface.create("sans-serif", Typeface.BOLD)

    // ================================================================== UI building

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        CrashHandler.install(this)

        glView = MeshGLView(this)
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

        val column = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(C_BG)
            layoutDirection = View.LAYOUT_DIRECTION_LTR   // the toolbar stays on the left, also in Arabic
        }
        column.addView(buildTopBar(), LinearLayout.LayoutParams(MATCH_PARENT, px(112)))

        val middle = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL }
        middle.addView(buildLeftToolbar(), LinearLayout.LayoutParams(px(72), MATCH_PARENT))
        middle.addView(glView, LinearLayout.LayoutParams(0, MATCH_PARENT, 1f))
        middle.addView(buildRightPanel(), LinearLayout.LayoutParams(px(280), MATCH_PARENT))
        column.addView(middle, LinearLayout.LayoutParams(MATCH_PARENT, 0, 1f))

        statusText = TextView(this).apply {
            setTextColor(C_SECONDARY)
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 11f)
            gravity = Gravity.CENTER_VERTICAL
            setPadding(px(12), 0, px(12), 0)
            setBackgroundColor(C_STATUS)
        }
        column.addView(statusText, LinearLayout.LayoutParams(MATCH_PARENT, px(32)))

        val root = FrameLayout(this)
        root.addView(column, FrameLayout.LayoutParams(MATCH_PARENT, MATCH_PARENT))
        root.addView(reportScroll, FrameLayout.LayoutParams(MATCH_PARENT, MATCH_PARENT))
        setContentView(root)

        val report = buildReport()
        if (report.isNotEmpty()) {
            reportText.text = report + "\n(tap to close)"
            reportScroll.visibility = View.VISIBLE
        }
        applyInfo("0 0 0 0 0 0")

        glView.setRenderer(object : GLSurfaceView.Renderer {
            override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {
                if (!NativeBridge.ok) return
                NativeBridge.onSurfaceCreated()
                val info = NativeBridge.meshInfo()
                this@MainActivity.runOnUiThread { applyInfo(info) }
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
        glView.onGestureEnd = { runOnGl { } }   // refresh the numbers after every gesture

        // Initial state: Object Mode, Select (tap), Move, Vertex.
        stylePill(pillObject, true)
        stylePill(pillEdit, false)
        setMethodTiles(0)
        setToolTiles(0)
        setElementTiles(0)
        for (t in editOnlyTiles) t.setDim(true)
        val density = resources.displayMetrics.density
        runOnGl {
            NativeBridge.setDensity(density)
            NativeBridge.setEditMode(false)
            NativeBridge.setElement(0)
            NativeBridge.setMethod(0)
            NativeBridge.setExtend(false)
            NativeBridge.setTool(0)
        }
    }

    private fun buildTopBar(): View {
        val bar = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setBackgroundColor(C_BAR)
            setPadding(px(20), 0, px(20), 0)
        }
        val title = TextView(this).apply {
            setText(R.string.title_bar)
            setTextColor(C_WHITE)
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 24f)
            typeface = bold
            maxLines = 1
        }
        bar.addView(title, LinearLayout.LayoutParams(0, WRAP_CONTENT, 1f))
        pillObject = pill(R.string.mode_object) { setEditMode(false) }
        pillEdit = pill(R.string.mode_edit) { setEditMode(true) }
        val lp1 = LinearLayout.LayoutParams(WRAP_CONTENT, px(52)).apply { rightMargin = px(10) }
        bar.addView(pillObject, lp1)
        bar.addView(pillEdit, LinearLayout.LayoutParams(WRAP_CONTENT, px(52)))
        return bar
    }

    private fun buildLeftToolbar(): View {
        val list = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER_HORIZONTAL
            setPadding(0, px(8), 0, px(8))
        }
        // The 10 tools of the spec, in order.
        tSelect = editTile(ToolIcon.SELECT, R.string.tool_select) { setMethod(0) }
        tMove = editTile(ToolIcon.MOVE, R.string.tool_move) { setTool(0) }
        tRotate = editTile(ToolIcon.ROTATE, R.string.tool_rotate) { setTool(1) }
        tScale = editTile(ToolIcon.SCALE, R.string.tool_scale) { setTool(2) }
        tVertex = editTile(ToolIcon.VERTEX, R.string.elem_vertex) { setElement(0) }
        tEdge = editTile(ToolIcon.EDGE, R.string.elem_edge) { setElement(1) }
        tFace = editTile(ToolIcon.FACE, R.string.elem_face) { setElement(2) }
        tBox = editTile(ToolIcon.BOX, R.string.method_box) { setMethod(1) }
        tLasso = editTile(ToolIcon.LASSO, R.string.method_lasso) { setMethod(2) }
        // Slot 10: the existing "Cube" action (puts the default cube back). The label says Cube on purpose.
        val tCube = Tile(ToolIcon.CUBE, R.string.btn_cube) {
            runOnGl { NativeBridge.resetCube() }
            toast(getString(R.string.msg_cube))
        }
        for (t in listOf(tSelect, tMove, tRotate, tScale, tVertex, tEdge, tFace, tBox, tLasso, tCube)) addTile(list, t)

        if (SHOW_EXTRA_ACTIONS) {
            list.addView(View(this).apply { setBackgroundColor(C_DIVIDER) },
                LinearLayout.LayoutParams(px(48), px(1)).apply { topMargin = px(6); bottomMargin = px(6) })
            addTile(list, Tile(ToolIcon.UNDO, R.string.act_undo) { runOnGl { NativeBridge.undo() } })
            addTile(list, Tile(ToolIcon.REDO, R.string.act_redo) { runOnGl { NativeBridge.redo() } })
            addTile(list, Tile(ToolIcon.OPEN, R.string.btn_open) { openGlbPicker() })
            addTile(list, Tile(ToolIcon.FRAME, R.string.btn_frame) { runOnGl { NativeBridge.frame() } })
            addTile(list, editTile(ToolIcon.ALL, R.string.sel_all) { runOnGl { NativeBridge.selectAll() } })
            addTile(list, editTile(ToolIcon.NONE, R.string.sel_none) { runOnGl { NativeBridge.selectNone() } })
            val add = editTile(ToolIcon.ADD, R.string.sel_add) { toggleExtend() }
            tAdd = add
            addTile(list, add)
        }

        return ScrollView(this).apply {
            setBackgroundColor(C_TOOLBAR)
            isVerticalScrollBarEnabled = false
            addView(list, FrameLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT))
        }
    }

    private fun buildRightPanel(): View {
        val panel = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(C_RIGHT)
            setPadding(px(16), px(16), px(16), px(16))
        }
        panel.addView(TextView(this).apply {
            setText(R.string.props_header)
            setTextColor(C_ORANGE)
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 13f)
            typeface = bold
            maxLines = 1
        }, LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT))
        panel.addView(View(this).apply { setBackgroundColor(C_DIVIDER) },
            LinearLayout.LayoutParams(MATCH_PARENT, px(1)).apply { topMargin = px(10); bottomMargin = px(14) })
        panel.addView(TextView(this).apply {
            setText(R.string.mesh_info)
            setTextColor(C_WHITE)
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 12f)
            typeface = bold
        }, LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT).apply { bottomMargin = px(8) })

        valueV = infoRow(panel, R.string.lbl_v)
        valueF = infoRow(panel, R.string.lbl_f)
        valueE = infoRow(panel, R.string.lbl_edges)
        return panel
    }

    private fun infoRow(parent: LinearLayout, labelRes: Int): TextView {
        val row = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL }
        row.addView(TextView(this).apply {
            setText(labelRes)
            setTextColor(C_SECONDARY)
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 12f)
        }, LinearLayout.LayoutParams(0, WRAP_CONTENT, 1f))
        val value = TextView(this).apply {
            setTextColor(C_WHITE)
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 12f)
            gravity = Gravity.END
        }
        row.addView(value, LinearLayout.LayoutParams(WRAP_CONTENT, WRAP_CONTENT))
        parent.addView(row, LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT).apply { bottomMargin = px(6) })
        return value
    }

    // ------------------------------------------------------------------ tiles and pills

    /** A 64x64 toolbar tile: icon on top, label under it (11sp, shrinks if the word does not fit). */
    private inner class Tile(kind: Int, textRes: Int, onClick: () -> Unit) {
        val view = LinearLayout(this@MainActivity)
        private val icon = ToolIcon(this@MainActivity, kind)
        private val label = TextView(this@MainActivity)

        init {
            view.orientation = LinearLayout.VERTICAL
            view.gravity = Gravity.CENTER_HORIZONTAL
            view.isClickable = true
            view.setOnClickListener { onClick() }
            view.addView(icon, LinearLayout.LayoutParams(px(30), px(30)).apply { topMargin = px(2) })
            label.setText(textRes)
            label.gravity = Gravity.CENTER
            label.maxLines = 1
            label.includeFontPadding = false
            label.setAutoSizeTextTypeUniformWithConfiguration(6, 11, 1, TypedValue.COMPLEX_UNIT_SP)
            view.addView(label, LinearLayout.LayoutParams(MATCH_PARENT, px(28)))
            setActive(false)
        }

        fun setActive(active: Boolean) {
            label.setTextColor(if (active) C_WHITE else C_SECONDARY)
            icon.tint = if (active) C_WHITE else C_SECONDARY
            if (active) {
                label.background = GradientDrawable().apply {
                    cornerRadius = px(6).toFloat()
                    setColor(C_ORANGE)
                }
            } else {
                label.background = null
            }
        }

        fun setDim(dim: Boolean) {
            view.alpha = if (dim) 0.35f else 1f
        }
    }

    private fun addTile(parent: LinearLayout, t: Tile) {
        parent.addView(t.view, LinearLayout.LayoutParams(px(64), px(64)).apply {
            topMargin = px(3)
            bottomMargin = px(3)
        })
    }

    /** A tile that only works in Edit Mode (dimmed and ignored in Object Mode). */
    private fun editTile(kind: Int, textRes: Int, action: () -> Unit): Tile {
        val t = Tile(kind, textRes) { if (editOn) action() }
        editOnlyTiles.add(t)
        return t
    }

    private fun pill(textRes: Int, onClick: () -> Unit): TextView = TextView(this).apply {
        setText(textRes)
        setTextSize(TypedValue.COMPLEX_UNIT_SP, 12f)
        typeface = bold
        gravity = Gravity.CENTER
        maxLines = 1
        setPadding(px(26), 0, px(26), 0)
        setOnClickListener { onClick() }
    }

    private fun stylePill(t: TextView, active: Boolean) {
        t.background = GradientDrawable().apply {
            cornerRadius = px(26).toFloat()
            setColor(if (active) C_ORANGE else C_PILL_OFF)
        }
        t.setTextColor(if (active) C_WHITE else C_DARK_TEXT)
    }

    // ------------------------------------------------------------------ editing controls

    private fun setEditMode(on: Boolean) {
        editOn = on
        stylePill(pillObject, !on)
        stylePill(pillEdit, on)
        for (t in editOnlyTiles) t.setDim(!on)
        runOnGl { NativeBridge.setEditMode(on) }
    }

    private fun setMethodTiles(m: Int) {
        tSelect.setActive(m == 0)
        tBox.setActive(m == 1)
        tLasso.setActive(m == 2)
    }

    private fun setToolTiles(t: Int) {
        tMove.setActive(t == 0)
        tRotate.setActive(t == 1)
        tScale.setActive(t == 2)
    }

    private fun setElementTiles(e: Int) {
        tVertex.setActive(e == 0)
        tEdge.setActive(e == 1)
        tFace.setActive(e == 2)
    }

    private fun setMethod(m: Int) {
        setMethodTiles(m)
        runOnGl { NativeBridge.setMethod(m) }
    }

    private fun setTool(t: Int) {
        setToolTiles(t)
        runOnGl { NativeBridge.setTool(t) }
    }

    private fun setElement(e: Int) {
        setElementTiles(e)
        runOnGl { NativeBridge.setElement(e) }
    }

    private fun toggleExtend() {
        extendOn = !extendOn
        val v = extendOn
        tAdd?.setActive(v)
        runOnGl { NativeBridge.setExtend(v) }
    }

    // ------------------------------------------------------------------ live numbers

    /** [info] = "V F Edges T Sel Undo" (numbers separated by spaces) from the C++ core. */
    private fun applyInfo(info: String) {
        val p = info.trim().split(' ')
        if (p.size < 6) return
        valueV.text = p[0]
        valueF.text = p[1]
        valueE.text = p[2]
        statusText.text = getString(R.string.status_fmt, p[0], p[1], p[4], p[5])
    }

    private fun toast(msg: String) {
        Toast.makeText(this, msg, Toast.LENGTH_SHORT).show()
    }

    /** Runs [block] on the GL thread, redraws, then refreshes the numbers on screen. */
    private fun runOnGl(block: () -> Unit) {
        if (!NativeBridge.ok) return
        glView.queueEvent {
            block()
            val info = NativeBridge.meshInfo()
            this@MainActivity.runOnUiThread { applyInfo(info) }
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
        toast(getString(R.string.msg_loading))
        val name = displayName(uri)
        Thread {
            val bytes: ByteArray? = try {
                contentResolver.openInputStream(uri)?.use { it.readBytes() }
            } catch (t: Throwable) {
                null
            }
            if (bytes == null) {
                runOnUiThread { toast(getString(R.string.msg_read_failed)) }
            } else {
                glView.queueEvent {
                    val err = try {
                        NativeBridge.loadGlb(bytes)
                    } catch (t: Throwable) {
                        t.toString()
                    }
                    val info = NativeBridge.meshInfo()
                    runOnUiThread {
                        applyInfo(info)
                        if (err.isEmpty()) toast(name)
                        else Toast.makeText(this, "Error: $err", Toast.LENGTH_LONG).show()
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
