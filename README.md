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

### Using M2
- Bottom bar: **Object / Edit**, **Undo / Redo**, **Open GLB**, **Cube**, **Frame**.
- In **Edit** a second row appears: Vertex / Edge / Face, Tap / Box / Lasso, **Add** (toggle: tapping or boxing adds to the
  selection, tapping a selected element removes it), Move / Rotate / Scale, All / None.
- Tap selects. With **Box** or **Lasso**, drag on empty space to draw the region. With **Tap**, dragging on empty space orbits.
- Drag a coloured gizmo handle (round tip on the red X, green Y, blue Z axis) to move / rotate / scale the selection.
  Rotate: turn your finger around the selection centre. Scale: drag the handle outwards or inwards.
- Two fingers: pinch to zoom, drag to pan.
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
