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
- M1 (this version): polygon mesh structure, grid + axes, orbit / pan / zoom camera, GLB import
  (node transforms, welded vertices, 700k triangle default limit), edges drawn over faces.

### Using M1
- Bottom bar: **Open GLB** (pick a `.glb`), **Cube** (back to the default cube), **Frame** (re-center the camera).
- One finger orbits, two fingers pinch-zoom and drag to pan.
- Test models: `samples/torus.glb`, `samples/two_cubes.glb` (regenerate with `python3 tools/make_samples.py`).
- Top-left text shows GPU, vertex / face / triangle counts, and the loaded file name or an error.

### Not supported yet (reported as an error message)
Draco / meshopt compression, quantized positions, external `.bin` buffers, plain `.gltf` files.

## TODO
- Add the GPL v3 `LICENSE` file (GitHub: Add file, Create new file, name it `LICENSE`, choose the license template).
