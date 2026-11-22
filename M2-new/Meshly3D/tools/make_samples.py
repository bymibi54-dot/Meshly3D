#!/usr/bin/env python3
"""Generates the small test models in ../samples (torus.glb, two_cubes.glb). Run: python3 tools/make_samples.py"""
import json, math, os, struct

def build_glb(gltf, blob):
    js = json.dumps(gltf, separators=(',', ':')).encode()
    js += b' ' * ((4 - len(js) % 4) % 4)
    blob += b'\0' * ((4 - len(blob) % 4) % 4)
    total = 12 + 8 + len(js) + 8 + len(blob)
    return (struct.pack('<III', 0x46546C67, 2, total) + struct.pack('<II', len(js), 0x4E4F534A) + js +
            struct.pack('<II', len(blob), 0x004E4942) + blob)

def signed_volume(pos, tris):
    v = 0.0
    for a, b, c in tris:
        A, B, C = pos[a], pos[b], pos[c]
        cx = B[1]*C[2] - B[2]*C[1]; cy = B[2]*C[0] - B[0]*C[2]; cz = B[0]*C[1] - B[1]*C[0]
        v += (A[0]*cx + A[1]*cy + A[2]*cz) / 6.0
    return v

def minmax(pos):
    return [min(p[i] for p in pos) for i in range(3)], [max(p[i] for p in pos) for i in range(3)]

def torus():
    U, V, R, r = 24, 12, 1.0, 0.3
    pos, nrm = [], []
    for i in range(U + 1):          # seam row duplicated on purpose: tests vertex welding
        u = 2 * math.pi * i / U
        for j in range(V + 1):
            v = 2 * math.pi * j / V
            pos.append(((R + r*math.cos(v))*math.cos(u), r*math.sin(v), (R + r*math.cos(v))*math.sin(u)))
            nrm.append((math.cos(v)*math.cos(u), math.sin(v), math.cos(v)*math.sin(u)))
    tris = []
    for i in range(U):
        for j in range(V):
            a = i*(V+1)+j; b = (i+1)*(V+1)+j; c = (i+1)*(V+1)+j+1; d = i*(V+1)+j+1
            tris += [(a, b, c), (a, c, d)]
    if signed_volume(pos, tris) < 0:
        tris = [(a, c, b) for a, b, c in tris]
    pb = b''.join(struct.pack('<3f', *p) for p in pos)
    nb = b''.join(struct.pack('<3f', *n) for n in nrm)
    ib = b''.join(struct.pack('<3H', *t) for t in tris)
    mn, mx = minmax(pos)
    s = math.sqrt(0.5)
    gltf = {
        "asset": {"version": "2.0"}, "scene": 0, "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0, "translation": [0, 0.5, 0], "rotation": [s, 0, 0, s]}],
        "meshes": [{"primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1}, "indices": 2, "mode": 4}]}],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": len(pos), "type": "VEC3", "min": mn, "max": mx},
            {"bufferView": 1, "componentType": 5126, "count": len(nrm), "type": "VEC3"},
            {"bufferView": 2, "componentType": 5123, "count": len(tris) * 3, "type": "SCALAR"}],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": len(pb)},
            {"buffer": 0, "byteOffset": len(pb), "byteLength": len(nb)},
            {"buffer": 0, "byteOffset": len(pb) + len(nb), "byteLength": len(ib)}],
        "buffers": [{"byteLength": len(pb) + len(nb) + len(ib)}]}
    return build_glb(gltf, pb + nb + ib)

def two_cubes():
    h = 0.5
    c = [(-h,-h,-h),(h,-h,-h),(h,h,-h),(-h,h,-h),(-h,-h,h),(h,-h,h),(h,h,h),(-h,h,h)]
    faces = [[4,5,6,7],[1,0,3,2],[5,1,2,6],[0,4,7,3],[7,6,2,3],[0,1,5,4]]
    pos, idx = [], []
    for f in faces:                 # 4 own vertices per face (like most exporters)
        base = len(pos)
        pos += [c[k] for k in f]
        idx += [base, base+1, base+2, base, base+2, base+3]
    pb = b''.join(struct.pack('<3f', *p) for p in pos)
    ib = struct.pack('<%dI' % len(idx), *idx)
    mn, mx = minmax(pos)
    gltf = {
        "asset": {"version": "2.0"}, "scene": 0, "scenes": [{"nodes": [0]}],
        "nodes": [
            {"children": [1, 2]},
            {"mesh": 0, "matrix": [2,0,0,0, 0,2,0,0, 0,0,2,0, -3,0,0,1]},     # big cube, matrix form
            {"mesh": 0, "translation": [3, 0, 0], "scale": [-1, 1, 1]}],      # mirrored cube: winding must flip
        "meshes": [{"primitives": [{"attributes": {"POSITION": 0}, "indices": 1}]}],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": len(pos), "type": "VEC3", "min": mn, "max": mx},
            {"bufferView": 1, "componentType": 5125, "count": len(idx), "type": "SCALAR"}],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": len(pb)},
            {"buffer": 0, "byteOffset": len(pb), "byteLength": len(ib)}],
        "buffers": [{"byteLength": len(pb) + len(ib)}]}
    return build_glb(gltf, pb + ib)

if __name__ == "__main__":
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "samples")
    os.makedirs(out, exist_ok=True)
    for name, data in (("torus.glb", torus()), ("two_cubes.glb", two_cubes())):
        with open(os.path.join(out, name), "wb") as f:
            f.write(data)
        print("wrote", name, len(data), "bytes")
