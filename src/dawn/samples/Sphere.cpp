#include "Sphere.h"
#include <cmath>
#include <random>
#include <cassert>

// Minimal vec3 operations used in the original JS code
static void vec3_copy(const float src[3], float dst[3]) {
    dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2];
}

static void vec3_normalize(float v[3]) {
    float len = std::sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
    if (len > 1e-8f) {
        v[0] /= len; v[1] /= len; v[2] /= len;
    }
}

SphereMesh Sphere::create(float radius, int widthSegments, int heightSegments, float randomness) {
    SphereMesh mesh;

    widthSegments = std::max(3, widthSegments);
    heightSegments = std::max(2, heightSegments);

    std::vector<float> vertices;
    std::vector<uint16_t> indices;
    vertices.reserve((widthSegments + 1) * (heightSegments + 1) * 8);

    float firstVertex[3] = {0.0f, 0.0f, 0.0f};
    float vertex[3] = {0.0f, 0.0f, 0.0f};
    float normal[3] = {0.0f, 0.0f, 0.0f};

    int index = 0;
    std::vector<std::vector<int>> grid;
    grid.reserve(heightSegments + 1);

    // RNG for randomness
    std::mt19937 rng(12345);
    std::uniform_real_distribution<float> dist(-0.5f, 0.5f);

    // generate vertices, normals and uvs
    for (int iy = 0; iy <= heightSegments; ++iy) {
        std::vector<int> verticesRow;
        verticesRow.reserve(widthSegments + 1);
        float v = static_cast<float>(iy) / static_cast<float>(heightSegments);

        float uOffset = 0.0f;
        if (iy == 0) {
            uOffset = 0.5f / static_cast<float>(widthSegments);
        } else if (iy == heightSegments) {
            uOffset = -0.5f / static_cast<float>(widthSegments);
        }

        for (int ix = 0; ix <= widthSegments; ++ix) {
            float u = static_cast<float>(ix) / static_cast<float>(widthSegments);

            if (ix == widthSegments) {
                vec3_copy(firstVertex, vertex);
            } else if (ix == 0 || (iy != 0 && iy != heightSegments)) {
                float rr = radius + dist(rng) * 2.0f * randomness * radius;

                vertex[0] = -rr * std::cos(u * static_cast<float>(M_PI) * 2.0f) * std::sin(v * static_cast<float>(M_PI));
                vertex[1] = rr * std::cos(v * static_cast<float>(M_PI));
                vertex[2] = rr * std::sin(u * static_cast<float>(M_PI) * 2.0f) * std::sin(v * static_cast<float>(M_PI));

                if (ix == 0) {
                    vec3_copy(vertex, firstVertex);
                }
            }

            // position
            vertices.push_back(vertex[0]);
            vertices.push_back(vertex[1]);
            vertices.push_back(vertex[2]);
            vertices.push_back(1.0f);
        
        
            // normal (copy vertex and normalize)
            normal[0] = vertex[0]; normal[1] = vertex[1]; normal[2] = vertex[2];
            vec3_normalize(normal);
            vertices.push_back(normal[0]); vertices.push_back(normal[1]); vertices.push_back(normal[2]);
        
            // uv
            vertices.push_back(u + uOffset);
            vertices.push_back(1.0f - v);
        
            verticesRow.push_back(index++);
         
        }

        grid.push_back(std::move(verticesRow));
    }

    // indices
    for (int iy = 0; iy < heightSegments; ++iy) {
        for (int ix = 0; ix < widthSegments; ++ix) {
            int a = grid[iy][ix + 1];
            int b = grid[iy][ix];
            int c = grid[iy + 1][ix];
            int d = grid[iy + 1][ix + 1];

            if (iy != 0) {
                indices.push_back(static_cast<uint16_t>(a));
                indices.push_back(static_cast<uint16_t>(b));
                indices.push_back(static_cast<uint16_t>(d));
            }
            if (iy != heightSegments - 1) {
                indices.push_back(static_cast<uint16_t>(b));
                indices.push_back(static_cast<uint16_t>(c));
                indices.push_back(static_cast<uint16_t>(d));
            }
        }
    }

    mesh.vertices = std::move(vertices);
    mesh.indices = std::move(indices);

    return mesh;
}
