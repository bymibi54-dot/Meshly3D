# Meshly3D

Open-source 3D modeling app for Android (inspired by Blender). License: GPL v3.

- Stack: Kotlin + C++ (NDK) + OpenGL ES 3.x
- Min Android: 10 (API 29)
- Package: `com.meshly3d.app` · Project file extension: `.meshly`

## Build

Build **only with GitHub Actions** (push to `main`, tab Actions, download artifact `Meshly3D-debug-apk`).
Phone build apps do not compile the C++ core (`libmeshly_native.so` would be missing).
The workflow first runs the C++ unit tests (`tests/`), then builds the APK and checks the `.so` is inside.

## Status

- M0 done: cloud build + renderer starts.
- M1 done: polygon mesh, grid + axes, orbit / pan / zoom, GLB import (700k triangle default limit), edges over faces.
- M2 (this version): **Edit mode**. Selection of vertices, edges, faces (tap, box, lasso, add-to-selection),
  gizmo with Move / Rotate / Scale on the X, Y, Z axes, undo / redo of vertex moves, camera-side lighting.

### Using M2 (UI v2, layout from the UI spec)
- **Top bar:** title "Meshly3D M2" and the **Object Mode / Edit Mode** pills (active = orange `#FF6A00`).
- **Left toolbar (72 px, 10 tools of 64x64):** Select, Move, Rotate, Scale, Vertex, Edge, Face, Box, Lasso, Cube.
  The active tool of each group has an orange label: Select / Box / Lasso (how to select), Move / Rotate / Scale (gizmo tool),
  Vertex / Edge / Face (what to select). They work in Edit Mode only (dimmed in Object Mode). "Cube" puts the default cube back.
  Below the 10 tools, the actions that already existed stay reachable: Undo, Redo, Open GLB, Frame, All, None, Add
  (set `SHOW_EXTRA_ACTIONS = false` in `MainActivity.kt` to hide them).
- **Right panel (280 px):** "<< PROPERTIES" and **Mesh Info** (V / F / Edges, live).
- **Status bar (32 px):** `V | F | Sel | Undo`, live.
- Layout numbers are pixels of a 1080-px-wide screen, scaled to the device (`uiScale` in `MainActivity.kt`).
- Gizmo: drag the round handle on the red X, green Y or blue Z axis (Move, Rotate: turn the finger around the selection, Scale: pull out or in).
- Select with a tap; with **Box** or **Lasso** drag on empty space. **Add** (toggle): selections are added, tapping a selected element removes it.
- Two fingers: pinch to zoom, drag to pan. With **Select**, one finger on empty space orbits.
- Messages (loading, file name, errors) appear as short toasts.
- Vertices and edges are selectable when they belong to a face that looks towards the camera (hidden back faces are ignored).

### Tests
- `tests/loader_test.cpp` (+ `tests/edit_tests.h`): GLB loader, mesh, selection, picking, gizmo math, undo. Runs in CI with sanitizers.
- `tests/renderer_sim.cpp` + `tests/stubs/`: runs the real renderer / touch logic on a PC with a no-op GL. Build:
  `g++ -std=c++17 -fsanitize=address,undefined -Itests/stubs -Iapp/src/main/cpp tests/renderer_sim.cpp tests/stubs/gl_stub.cpp app/src/main/cpp/renderer.cpp app/src/main/cpp/mesh.cpp app/src/main/cpp/glb_loader.cpp -o renderer_sim && ./renderer_sim samples`

### Known limits of M2
- Undo / redo covers moving, rotating and scaling. Topology changes (extrude, bevel...) come in M3 with their own undo.
- The whole mesh is re-uploaded to the GPU while dragging: fine for small and medium models, slower near the 700k limit.
- Scale works along one axis at a time (no uniform scale handle yet).

### Not supported yet (reported as an error message)
Draco / meshopt compression, quantized positions, external `.bin` buffers, plain `.gltf` files.

## TODO
- Add the GPL v3 `LICENSE` file (GitHub: Add file, Create new file, name it `LICENSE`, choose the license template).
