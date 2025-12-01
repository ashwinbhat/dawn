#pragma once

#include <vector>
#include <cstdint>

struct SphereMesh {
    std::vector<float> vertices; // interleaved: position(3), normal(3), uv(2)
    std::vector<uint16_t> indices;
};

struct SphereLayout {
    static constexpr size_t vertexStride = 8 * sizeof(float);
    static constexpr size_t positionsOffset = 0;
    static constexpr size_t normalOffset = 3 * sizeof(float);
    static constexpr size_t uvOffset = 6 * sizeof(float);
};

class Sphere {
public:
    // radius, widthSegments, heightSegments, randomness
    static SphereMesh create(float radius, int widthSegments = 32, int heightSegments = 16, float randomness = 0.0f);
};
