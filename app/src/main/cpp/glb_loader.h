#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

#include "mesh.h"

/**
 * Loads every triangle mesh of a binary glTF (.glb) into one welded polygon mesh.
 * Applies node transforms. Returns "" on success, otherwise a short English error message.
 * Not supported (reported as errors): Draco/meshopt compression, quantized positions, external buffers.
 */
std::string loadGlb(const uint8_t* data, size_t size, size_t maxTriangles, Mesh& out);
