# Meshly3D

Open-source 3D modeling app for Android (inspired by Blender). License: GPL v3.

- Stack: Kotlin + C++ (NDK) + OpenGL ES 3.x
- Min Android: 10 (API 29)
- Package: `com.meshly3d.app` · Project file extension: `.meshly`

## M0 status

Goal: prove the cloud build works and the renderer starts.

1. Push this project to GitHub (branch `main`).
2. Open the **Actions** tab, workflow **Android build (M0)** (it also runs from "Run workflow").
3. When green, download the artifact **Meshly3D-debug-apk** and install it.
4. Expected on device: a grey cube on a dark background, one finger orbits, two fingers zoom,
   and the top-left text shows the OpenGL ES version and GPU name.

M0 is done when the three checks pass: build is green, APK installs, cube is visible.

## TODO
- Add the GPL v3 `LICENSE` file (GitHub: Add file → Create new file → name it `LICENSE` → "Choose a license template").
