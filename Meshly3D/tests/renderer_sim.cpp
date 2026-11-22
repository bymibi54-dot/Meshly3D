// Drives the real Renderer (touch state machine, overlays, gizmo drawing) on the host with a no-op GL.
// Build: see tests/README or the command in the project notes. Run under sanitizers.
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "renderer.h"

static int g_fail = 0;
#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
            ++g_fail;                                                   \
        }                                                               \
    } while (0)

static bool has(const std::string& s, const char* part) { return s.find(part) != std::string::npos; }

int main(int argc, char** argv) {
    const std::string dir = argc > 1 ? argv[1] : "samples";
    Renderer r;
    r.onSurfaceCreated();
    r.onSurfaceChanged(800, 600);
    r.setDensity(2.6f);
    r.onDrawFrame();
    CHECK(has(r.info(), "OpenGL ES 3.2"));
    CHECK(has(r.stats(), "V 8 | F 6 | T 12"));

    // ---- Object mode: touches only orbit, nothing is selected
    r.touchDown(400, 300);
    r.touchMove(450, 320);
    r.touchUp(450, 320);
    r.onDrawFrame();
    CHECK(!has(r.stats(), "Sel"));

    // ---- Edit mode, face select by tapping the middle of the screen (the cube is centred)
    r.frame();
    r.setEditMode(true);
    r.setElement(2);
    r.touchDown(400, 300);
    r.touchUp(400, 300);
    r.onDrawFrame();
    CHECK(has(r.stats(), "Sel 1 faces"));

    // ---- gizmo: find a start point that grabs a handle (scan around the centre), then drag it
    r.setTool(0);
    bool dragged = false;
    for (float y = 200; y <= 420 && !dragged; y += 12)
        for (float x = 250; x <= 560 && !dragged; x += 12) {
            r.touchDown(x, y);
            r.touchMove(x + 25, y);
            r.touchMove(x + 40, y);
            r.touchUp(x + 40, y);
            if (has(r.stats(), "Undo 1")) dragged = true;
            else if (has(r.stats(), "Sel 0")) {  // a miss on empty space cleared the selection: select again
                r.touchDown(400, 300);
                r.touchUp(400, 300);
            }
        }
    r.onDrawFrame();
    CHECK(dragged);
    r.undo();
    CHECK(has(r.stats(), "Undo 0"));
    r.redo();
    CHECK(has(r.stats(), "Undo 1"));

    // ---- box and lasso
    r.setElement(0);
    r.selectNone();
    r.setMethod(1);
    r.touchDown(5, 5);
    r.touchMove(300, 200);
    r.touchMove(795, 595);
    r.onDrawFrame();  // draws the region outline
    r.touchUp(795, 595);
    CHECK(has(r.stats(), "Sel 7 verts") || has(r.stats(), "Sel 8 verts"));  // far corner hidden (the cube was moved a bit)
    r.setMethod(2);
    r.selectNone();
    r.touchDown(100, 100);
    for (int i = 0; i < 20; ++i) r.touchMove(100 + i * 30, 100 + (i % 5) * 80);
    r.touchMove(700, 520);
    r.onDrawFrame();
    r.touchUp(700, 520);

    // ---- two-finger style cancel in the middle of a gizmo drag
    r.setMethod(0);
    r.touchDown(400, 300);
    r.touchCancel();
    r.onDrawFrame();

    // ---- a real model + random everything (sanitizers catch out-of-range access)
    std::ifstream f(dir + "/torus.glb", std::ios::binary);
    std::vector<uint8_t> glb((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    CHECK(r.loadGlb(glb.data(), glb.size()).empty());
    r.onDrawFrame();
    unsigned long long st = 0x2545F4914F6CDD1DULL;
    auto rnd = [&]() {
        st ^= st << 13;
        st ^= st >> 7;
        st ^= st << 17;
        return st;
    };
    auto rf = [&](float a, float b) { return a + (b - a) * float(rnd() % 10000) / 10000.0f; };
    for (int it = 0; it < 3000; ++it) {
        switch (rnd() % 14) {
            case 0: r.setEditMode(rnd() % 4 != 0); break;
            case 1: r.setElement(int(rnd() % 3)); break;
            case 2: r.setMethod(int(rnd() % 3)); break;
            case 3: r.setTool(int(rnd() % 3)); break;
            case 4: r.setExtend(rnd() % 2 == 0); break;
            case 5: r.undo(); break;
            case 6: r.redo(); break;
            case 7: r.selectAll(); break;
            case 8: r.selectNone(); break;
            case 9: r.orbit(rf(-50, 50), rf(-50, 50)); r.pan(rf(-30, 30), rf(-30, 30)); r.zoom(rf(0.5f, 1.8f)); break;
            case 10: r.frame(); break;
            case 11: r.touchCancel(); break;
            case 12: if (rnd() % 20 == 0) r.resetToCube(); else if (rnd() % 20 == 0) r.loadGlb(glb.data(), glb.size()); break;
            default: {
                const float x = rf(0, 800), y = rf(0, 600);
                r.touchDown(x, y);
                const int moves = int(rnd() % 6);
                for (int k = 0; k < moves; ++k) r.touchMove(rf(0, 800), rf(0, 600));
                if (rnd() % 5 == 0) r.touchCancel(); else r.touchUp(rf(0, 800), rf(0, 600));
                break;
            }
        }
        if (it % 3 == 0) r.onDrawFrame();
    }
    r.onDrawFrame();
    std::printf("final: %s\n", r.stats().c_str());

    if (g_fail) {
        std::printf("%d CHECK(S) FAILED\n", g_fail);
        return 1;
    }
    std::printf("RENDERER SIM PASSED\n");
    return 0;
}
