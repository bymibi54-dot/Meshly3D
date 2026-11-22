#include "glb_loader.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <numeric>
#include <vector>

namespace {

// ---------------------------------------------------------------- tiny JSON parser
struct Json {
    enum Type { Null, Bool, Num, Str, Arr, Obj } type = Null;
    double num = 0.0;
    bool b = false;
    std::string str;
    std::vector<Json> arr;
    std::vector<std::string> keys;
    std::vector<Json> vals;

    const Json* get(const char* key) const {
        if (type != Obj) return nullptr;
        for (size_t i = 0; i < keys.size(); ++i)
            if (keys[i] == key) return &vals[i];
        return nullptr;
    }
    const Json* at(size_t i) const { return (type == Arr && i < arr.size()) ? &arr[i] : nullptr; }
    double numOr(double d) const { return type == Num ? num : d; }
};

class JsonParser {
public:
    JsonParser(const char* b, const char* e) : p_(b), end_(e) {}

    bool parse(Json& out, std::string& err) {
        if (!value(out, 0)) {
            err = err_.empty() ? "Invalid JSON" : err_;
            return false;
        }
        return true;
    }

private:
    const char* p_;
    const char* end_;
    std::string err_;

    bool fail(const char* m) {
        if (err_.empty()) err_ = m;
        return false;
    }
    void skipWs() {
        while (p_ < end_ && (*p_ == ' ' || *p_ == '\n' || *p_ == '\r' || *p_ == '\t')) ++p_;
    }
    bool literal(const char* word, Json& o, Json::Type t, bool bv) {
        const size_t n = std::strlen(word);
        if (static_cast<size_t>(end_ - p_) < n || std::memcmp(p_, word, n) != 0) return fail("Invalid JSON literal");
        p_ += n;
        o.type = t;
        o.b = bv;
        return true;
    }
    bool string(std::string& s) {
        ++p_;  // opening quote
        while (p_ < end_) {
            char c = *p_++;
            if (c == '"') return true;
            if (c == '\\') {
                if (p_ >= end_) break;
                char e = *p_++;
                switch (e) {
                    case 'n': s.push_back('\n'); break;
                    case 't': s.push_back('\t'); break;
                    case 'r': s.push_back('\r'); break;
                    case 'b': s.push_back('\b'); break;
                    case 'f': s.push_back('\f'); break;
                    case 'u':
                        if (end_ - p_ < 4) return fail("Bad JSON escape");
                        p_ += 4;
                        s.push_back('?');
                        break;
                    default: s.push_back(e); break;
                }
            } else {
                s.push_back(c);
            }
        }
        return fail("Unterminated JSON string");
    }
    bool number(Json& o) {
        std::string tmp;
        while (p_ < end_ && std::strchr("+-0123456789.eE", *p_) != nullptr && *p_ != '\0') tmp.push_back(*p_++);
        if (tmp.empty()) return fail("Invalid JSON value");
        o.type = Json::Num;
        o.num = std::strtod(tmp.c_str(), nullptr);
        return true;
    }
    bool value(Json& o, int depth) {
        if (depth > 64) return fail("JSON nested too deep");
        skipWs();
        if (p_ >= end_) return fail("JSON ended early");
        const char c = *p_;
        if (c == '{') return object(o, depth);
        if (c == '[') return array(o, depth);
        if (c == '"') {
            o.type = Json::Str;
            return string(o.str);
        }
        if (c == 't') return literal("true", o, Json::Bool, true);
        if (c == 'f') return literal("false", o, Json::Bool, false);
        if (c == 'n') return literal("null", o, Json::Null, false);
        return number(o);
    }
    bool array(Json& o, int depth) {
        o.type = Json::Arr;
        ++p_;
        skipWs();
        if (p_ < end_ && *p_ == ']') {
            ++p_;
            return true;
        }
        while (true) {
            o.arr.emplace_back();
            if (!value(o.arr.back(), depth + 1)) return false;
            skipWs();
            if (p_ >= end_) return fail("JSON ended early");
            if (*p_ == ',') {
                ++p_;
                continue;
            }
            if (*p_ == ']') {
                ++p_;
                return true;
            }
            return fail("Expected , or ] in JSON");
        }
    }
    bool object(Json& o, int depth) {
        o.type = Json::Obj;
        ++p_;
        skipWs();
        if (p_ < end_ && *p_ == '}') {
            ++p_;
            return true;
        }
        while (true) {
            skipWs();
            if (p_ >= end_ || *p_ != '"') return fail("Expected string key in JSON");
            std::string key;
            if (!string(key)) return false;
            skipWs();
            if (p_ >= end_ || *p_ != ':') return fail("Expected : in JSON");
            ++p_;
            o.keys.push_back(std::move(key));
            o.vals.emplace_back();
            if (!value(o.vals.back(), depth + 1)) return false;
            skipWs();
            if (p_ >= end_) return fail("JSON ended early");
            if (*p_ == ',') {
                ++p_;
                continue;
            }
            if (*p_ == '}') {
                ++p_;
                return true;
            }
            return fail("Expected , or } in JSON");
        }
    }
};

// ---------------------------------------------------------------- matrices (column-major)
using Mat4 = std::array<double, 16>;

Mat4 identity() {
    Mat4 m{};
    m[0] = m[5] = m[10] = m[15] = 1.0;
    return m;
}

Mat4 mul(const Mat4& a, const Mat4& b) {
    Mat4 r{};
    for (int c = 0; c < 4; ++c)
        for (int row = 0; row < 4; ++row) {
            double s = 0.0;
            for (int k = 0; k < 4; ++k) s += a[k * 4 + row] * b[c * 4 + k];
            r[c * 4 + row] = s;
        }
    return r;
}

double det3(const Mat4& m) {
    // columns c0 = m[0..2], c1 = m[4..6], c2 = m[8..10]; det = c0 . (c1 x c2)
    const double cx = m[5] * m[10] - m[6] * m[9];
    const double cy = m[6] * m[8] - m[4] * m[10];
    const double cz = m[4] * m[9] - m[5] * m[8];
    return m[0] * cx + m[1] * cy + m[2] * cz;
}

void readArr(const Json* a, double* dst, size_t n) {
    if (!a || a->type != Json::Arr || a->arr.size() != n) return;
    for (size_t i = 0; i < n; ++i) dst[i] = a->arr[i].numOr(dst[i]);
}

Mat4 localMatrix(const Json& node) {
    const Json* mat = node.get("matrix");
    if (mat && mat->type == Json::Arr && mat->arr.size() == 16) {
        Mat4 m;
        for (size_t i = 0; i < 16; ++i) m[i] = mat->arr[i].numOr(0.0);
        return m;
    }
    double t[3] = {0, 0, 0}, q[4] = {0, 0, 0, 1}, s[3] = {1, 1, 1};
    readArr(node.get("translation"), t, 3);
    readArr(node.get("rotation"), q, 4);
    readArr(node.get("scale"), s, 3);
    const double len = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    if (len > 0.0) {
        for (double& v : q) v /= len;
    } else {
        q[0] = q[1] = q[2] = 0.0;
        q[3] = 1.0;
    }
    const double x = q[0], y = q[1], z = q[2], w = q[3];
    const double r00 = 1 - 2 * (y * y + z * z), r01 = 2 * (x * y - z * w), r02 = 2 * (x * z + y * w);
    const double r10 = 2 * (x * y + z * w), r11 = 1 - 2 * (x * x + z * z), r12 = 2 * (y * z - x * w);
    const double r20 = 2 * (x * z - y * w), r21 = 2 * (y * z + x * w), r22 = 1 - 2 * (x * x + y * y);
    Mat4 m{};
    m[0] = r00 * s[0]; m[1] = r10 * s[0]; m[2] = r20 * s[0];
    m[4] = r01 * s[1]; m[5] = r11 * s[1]; m[6] = r21 * s[1];
    m[8] = r02 * s[2]; m[9] = r12 * s[2]; m[10] = r22 * s[2];
    m[12] = t[0]; m[13] = t[1]; m[14] = t[2]; m[15] = 1.0;
    return m;
}

bool asIndex(const Json* j, size_t& out) {
    if (!j || j->type != Json::Num) return false;
    const double d = j->num;
    if (!(d >= 0.0 && d < 1e9)) return false;
    out = static_cast<size_t>(d);
    return true;
}

// ---------------------------------------------------------------- glTF reading
struct Loader {
    const Json* nodes = nullptr;
    const Json* meshes = nullptr;
    const Json* accessors = nullptr;
    const Json* views = nullptr;
    const uint8_t* bin = nullptr;
    size_t binLen = 0;
    Mesh* out = nullptr;
    size_t maxTris = 0;
    size_t triCount = 0;
    size_t visits = 0;
    std::string err;

    bool fail(const std::string& m) {
        if (err.empty()) err = m;
        return false;
    }

    // Finds the bytes of an accessor. indices=true accepts SCALAR uint8/16/32, otherwise VEC3 float.
    bool locate(size_t idx, bool indices, size_t& start, size_t& stride, size_t& count, int& comp, size_t& elemSize) {
        const Json* acc = accessors ? accessors->at(idx) : nullptr;
        if (!acc || acc->type != Json::Obj) return fail("Bad accessor index");
        const Json* type = acc->get("type");
        const char* want = indices ? "SCALAR" : "VEC3";
        if (!type || type->type != Json::Str || type->str != want)
            return fail(std::string("Accessor type must be ") + want);
        const Json* ct = acc->get("componentType");
        comp = ct ? static_cast<int>(ct->numOr(0)) : 0;
        if (indices) {
            if (comp == 5121) elemSize = 1;
            else if (comp == 5123) elemSize = 2;
            else if (comp == 5125) elemSize = 4;
            else return fail("Unsupported index type");
        } else {
            if (comp != 5126) return fail("Only float positions are supported (quantized meshes are not)");
            elemSize = 12;
        }
        size_t cnt = 0;
        if (!asIndex(acc->get("count"), cnt) || cnt > 200000000u) return fail("Bad accessor count");
        count = cnt;
        size_t bv = 0;
        if (!asIndex(acc->get("bufferView"), bv)) return fail("Accessors without bufferView (sparse) are not supported");
        const Json* view = views ? views->at(bv) : nullptr;
        if (!view || view->type != Json::Obj) return fail("Bad bufferView index");
        size_t bufIdx = 0;
        const Json* buf = view->get("buffer");
        if (buf && (!asIndex(buf, bufIdx) || bufIdx != 0)) return fail("Only the embedded GLB buffer is supported");
        size_t viewOff = 0, accOff = 0, strideProp = 0;
        const Json* vo = view->get("byteOffset");
        if (vo && !asIndex(vo, viewOff)) return fail("Bad byteOffset");
        const Json* ao = acc->get("byteOffset");
        if (ao && !asIndex(ao, accOff)) return fail("Bad byteOffset");
        const Json* sp = view->get("byteStride");
        if (sp && !asIndex(sp, strideProp)) return fail("Bad byteStride");
        stride = strideProp > 0 ? strideProp : elemSize;
        start = viewOff + accOff;
        if (count > 0) {
            const size_t last = start + (count - 1) * stride + elemSize;
            if (last > binLen || last < start) return fail("Accessor reads outside the file");
        }
        return true;
    }

    bool readPositions(size_t idx, std::vector<float>& outPos) {
        size_t start, stride, count, elem;
        int comp;
        if (!locate(idx, false, start, stride, count, comp, elem)) return false;
        outPos.resize(count * 3);
        for (size_t i = 0; i < count; ++i) {
            std::memcpy(&outPos[i * 3], bin + start + i * stride, 12);
            for (int k = 0; k < 3; ++k)
                if (!std::isfinite(outPos[i * 3 + k])) return fail("Invalid vertex data");
        }
        return true;
    }

    bool readIndices(size_t idx, std::vector<uint32_t>& outIdx) {
        size_t start, stride, count, elem;
        int comp;
        if (!locate(idx, true, start, stride, count, comp, elem)) return false;
        outIdx.resize(count);
        for (size_t i = 0; i < count; ++i) {
            const uint8_t* p = bin + start + i * stride;
            if (comp == 5121) {
                outIdx[i] = *p;
            } else if (comp == 5123) {
                uint16_t v;
                std::memcpy(&v, p, 2);
                outIdx[i] = v;
            } else {
                uint32_t v;
                std::memcpy(&v, p, 4);
                outIdx[i] = v;
            }
        }
        return true;
    }

    bool addMesh(const Json& mesh, const Mat4& world) {
        const Json* prims = mesh.get("primitives");
        if (!prims || prims->type != Json::Arr) return true;
        const bool flip = det3(world) < 0.0;  // mirrored transform reverses the winding
        for (const Json& prim : prims->arr) {
            if (prim.type != Json::Obj) continue;
            const Json* mode = prim.get("mode");
            if (mode && static_cast<int>(mode->numOr(4)) != 4) continue;  // only triangle lists
            const Json* attrs = prim.get("attributes");
            size_t posIdx = 0;
            if (!attrs || !asIndex(attrs->get("POSITION"), posIdx)) continue;

            std::vector<float> pos;
            if (!readPositions(posIdx, pos)) return false;
            const size_t vcount = pos.size() / 3;

            std::vector<uint32_t> idx;
            size_t ii = 0;
            if (asIndex(prim.get("indices"), ii)) {
                if (!readIndices(ii, idx)) return false;
            } else {
                idx.resize(vcount);
                std::iota(idx.begin(), idx.end(), 0u);
            }
            const size_t tris = idx.size() / 3;
            if (triCount + tris > maxTris)
                return fail("Model has too many triangles (limit " + std::to_string(maxTris) + ")");
            if (out->positions.size() + vcount > 20000000u) return fail("Model has too many vertices");

            const uint32_t base = static_cast<uint32_t>(out->positions.size());
            for (size_t i = 0; i < vcount; ++i) {
                const double x = pos[i * 3], y = pos[i * 3 + 1], z = pos[i * 3 + 2];
                Vec3f v;
                v.x = static_cast<float>(world[0] * x + world[4] * y + world[8] * z + world[12]);
                v.y = static_cast<float>(world[1] * x + world[5] * y + world[9] * z + world[13]);
                v.z = static_cast<float>(world[2] * x + world[6] * y + world[10] * z + world[14]);
                out->positions.push_back(v);
            }
            for (size_t t = 0; t < tris; ++t) {
                const uint32_t a = idx[t * 3], b = idx[t * 3 + 1], c = idx[t * 3 + 2];
                if (a >= vcount || b >= vcount || c >= vcount) return fail("Index out of range");
                const uint32_t f[3] = {base + a, base + (flip ? c : b), base + (flip ? b : c)};
                out->addFace(f, 3);
            }
            triCount += tris;
        }
        return true;
    }

    bool visit(size_t ni, const Mat4& parent, int depth) {
        if (depth > 64) return fail("Node hierarchy too deep");
        if (++visits > 200000) return fail("Too many nodes");
        const Json* n = nodes ? nodes->at(ni) : nullptr;
        if (!n || n->type != Json::Obj) return fail("Bad node index");
        const Mat4 world = mul(parent, localMatrix(*n));
        size_t mi = 0;
        if (asIndex(n->get("mesh"), mi)) {
            const Json* mesh = meshes ? meshes->at(mi) : nullptr;
            if (!mesh || mesh->type != Json::Obj) return fail("Bad mesh index");
            if (!addMesh(*mesh, world)) return false;
        }
        const Json* ch = n->get("children");
        if (ch && ch->type == Json::Arr) {
            for (const Json& c : ch->arr) {
                size_t ci = 0;
                if (!asIndex(&c, ci)) return fail("Bad child index");
                if (!visit(ci, world, depth + 1)) return false;
            }
        }
        return true;
    }
};

}  // namespace

std::string loadGlb(const uint8_t* data, size_t size, size_t maxTriangles, Mesh& out) {
    out.clear();
    if (!data || size < 12) return "File is too small to be a GLB";
    auto rd32 = [&](size_t off) {
        uint32_t v;
        std::memcpy(&v, data + off, 4);
        return v;
    };
    if (rd32(0) != 0x46546C67u) return "Not a binary glTF file (choose a .glb file)";
    if (rd32(4) != 2u) return "Unsupported glTF version";
    const size_t total = std::min<size_t>(rd32(8), size);

    const uint8_t* jsonPtr = nullptr;
    size_t jsonLen = 0;
    const uint8_t* bin = nullptr;
    size_t binLen = 0;
    size_t off = 12;
    while (off + 8 <= total) {
        const uint32_t len = rd32(off);
        const uint32_t type = rd32(off + 4);
        off += 8;
        if (len > total - off) return "Corrupt GLB chunk";
        if (type == 0x4E4F534Au && !jsonPtr) {
            jsonPtr = data + off;
            jsonLen = len;
        } else if (type == 0x004E4942u && !bin) {
            bin = data + off;
            binLen = len;
        }
        off += len;
    }
    if (!jsonPtr) return "GLB has no JSON chunk";

    Json root;
    {
        JsonParser parser(reinterpret_cast<const char*>(jsonPtr), reinterpret_cast<const char*>(jsonPtr) + jsonLen);
        std::string perr;
        if (!parser.parse(root, perr)) return "glTF JSON error: " + perr;
    }
    if (root.type != Json::Obj) return "glTF JSON root must be an object";

    const Json* req = root.get("extensionsRequired");
    if (req && req->type == Json::Arr) {
        for (const Json& e : req->arr) {
            if (e.type != Json::Str) continue;
            if (e.str == "KHR_draco_mesh_compression") return "Draco-compressed models are not supported yet";
            if (e.str == "EXT_meshopt_compression") return "Meshopt-compressed models are not supported yet";
            if (e.str == "KHR_mesh_quantization") return "Quantized models are not supported yet";
        }
    }

    Loader L;
    L.nodes = root.get("nodes");
    L.meshes = root.get("meshes");
    L.accessors = root.get("accessors");
    L.views = root.get("bufferViews");
    L.bin = bin;
    L.binLen = binLen;
    L.out = &out;
    L.maxTris = maxTriangles;

    // Which nodes to start from: the default scene, else every node that is nobody's child.
    std::vector<size_t> roots;
    const Json* scenes = root.get("scenes");
    size_t sceneIdx = 0;
    asIndex(root.get("scene"), sceneIdx);
    const Json* scene = scenes ? scenes->at(sceneIdx) : nullptr;
    const Json* sceneNodes = scene ? scene->get("nodes") : nullptr;
    if (sceneNodes && sceneNodes->type == Json::Arr) {
        for (const Json& n : sceneNodes->arr) {
            size_t i = 0;
            if (asIndex(&n, i)) roots.push_back(i);
        }
    } else if (L.nodes && L.nodes->type == Json::Arr) {
        std::vector<bool> isChild(L.nodes->arr.size(), false);
        for (const Json& n : L.nodes->arr) {
            const Json* ch = n.get("children");
            if (!ch || ch->type != Json::Arr) continue;
            for (const Json& c : ch->arr) {
                size_t ci = 0;
                if (asIndex(&c, ci) && ci < isChild.size()) isChild[ci] = true;
            }
        }
        for (size_t i = 0; i < isChild.size(); ++i)
            if (!isChild[i]) roots.push_back(i);
    }

    for (size_t r : roots) {
        if (!L.visit(r, identity(), 0)) {
            out.clear();
            return L.err;
        }
    }
    if (out.faceCount() == 0) {
        out.clear();
        return "No triangle meshes found in this file";
    }

    Vec3f mn, mx;
    out.bounds(mn, mx);
    const double dx = mx.x - mn.x, dy = mx.y - mn.y, dz = mx.z - mn.z;
    const double diag = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (!(diag > 0.0) || !std::isfinite(diag)) {
        out.clear();
        return "Model has no size";
    }
    out.weld(static_cast<float>(diag * 1e-5));
    if (out.faceCount() == 0) {
        out.clear();
        return "Model has no valid triangles";
    }
    return "";
}
