// Copyright 2017 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <vector>
#include "Sphere.h"
#include "dawn/samples/SampleUtils.h"

#include "dawn/utils/ComboRenderPipelineDescriptor.h"
#include "dawn/utils/WGPUHelpers.h"

#define _SPHERE_
int primitveCount = 0;
class HelloMeshSample : public SampleBase {
  public:
    using SampleBase::SampleBase;

  private:
    bool SetupImpl() override {
  
    #ifdef _SPHERE_
        //Add sphere
        float radius = 1.0f;
        int widthSeg = 32;
        int heightSeg = 16;
        float randomness = 0.0f;
        int stride = 4 + 3 + 2;

        SphereMesh mesh = Sphere::create(radius, widthSeg, heightSeg, randomness);

        std::cout << "Generated sphere:\n";
        std::cout << "  vertices count: " << (mesh.vertices.size() / stride) << "\n";
        std::cout << "  indices count: " << mesh.indices.size() << "\n";

        // print first vertex
        if (!mesh.vertices.empty()) {
            std::cout << "first vertex pos: (" << mesh.vertices[0] << ", " << mesh.vertices[1] << ", " << mesh.vertices[2] << ")\n";
            std::cout << "first vertex normal: (" << mesh.vertices[3] << ", " << mesh.vertices[4] << ", " << mesh.vertices[5] << ")\n";
            std::cout << "first vertex uv: (" << mesh.vertices[6] << ", " << mesh.vertices[7] << ")\n";
        }

        vertexBuffer = dawn::utils::CreateBufferFromData(device, mesh.vertices.data(), mesh.vertices.size() * stride * sizeof(float),
                                                         wgpu::BufferUsage::Vertex);

        indexBuffer  = dawn::utils::CreateBufferFromData(device, mesh.indices.data(), mesh.indices.size() * sizeof(u_int16_t),
                                                         wgpu::BufferUsage::Index);
    
        primitveCount = mesh.indices.size();
    #else // triangle
        // float4 position, float3 normal
        static const float vertexData[] = {
            -0.5f, 0.5f, 0.0f, 1.0f,     1,0,0, 0,0,
             0.5f, -0.5f, 0.0f, 1.0f,    0,1,0, 0,1,  
             -0.5f, -0.5f, 0.0f, 1.0f,   0,0,1, 1,0,
             0.5f, 0.5f, 0.0f, 1.0f,     1,1,0, 1,1,
        };

        static const uint16_t indexData[] = { 0, 1, 2, 0, 3, 1};
        int stride = 4 + 3 + 2;

        vertexBuffer = dawn::utils::CreateBufferFromData(device, vertexData, sizeof(vertexData),
                                                         wgpu::BufferUsage::Vertex);

        indexBuffer  = dawn::utils::CreateBufferFromData(device, indexData, sizeof(indexData),
                                                         wgpu::BufferUsage::Index);

        primitveCount = sizeof(indexData)/sizeof(indexData[0]);
    #endif

        wgpu::ShaderModule module = dawn::utils::CreateShaderModule(device, R"(

        struct Vertex {
          @location(0) position: vec4f,
          @location(1) normal: vec3f,
          @location(2) uv:vec2f,
        };

        struct VSOut {
           @builtin(position) position: vec4f,
           @location(0) color: vec4f,
        };

        @vertex fn vs(vin: Vertex) -> VSOut
        {
            var vOut: VSOut;
            vOut.position = vin.position;
            //vOut.color = vec4(vin.normal, 1.0f);
            vOut.color = vec4(vin.uv, 1, 1);
            return vOut;
        }

        @fragment fn fs(vin: VSOut) -> @location(0) vec4f {
            return vin.color;
        }
    )");

        dawn::utils::ComboRenderPipelineDescriptor descriptor;
        descriptor.layout = nullptr;
        //primitive topology
        // descriptor.primitive.topology = wgpu::PrimitiveTopology::LineList;
        descriptor.vertex.module = module;
        descriptor.vertex.bufferCount = 1;
        // pos float4, normal float3
        descriptor.cBuffers[0].arrayStride = stride * sizeof(float);
        descriptor.cBuffers[0].attributeCount = 3;

        // pos attrib
        descriptor.cAttributes[0].format = wgpu::VertexFormat::Float32x4;
        // norm attrib
        descriptor.cAttributes[1].shaderLocation = 1;
        descriptor.cAttributes[1].offset = 4 * sizeof(float);
        descriptor.cAttributes[1].format = wgpu::VertexFormat::Float32x3;
      
        // uv attrib
        descriptor.cAttributes[2].shaderLocation = 2;
        descriptor.cAttributes[2].offset = (4+3) * sizeof(float);
        descriptor.cAttributes[2].format = wgpu::VertexFormat::Float32x2;

        descriptor.cFragment.module = module;
        descriptor.cTargets[0].format = GetPreferredSurfaceTextureFormat();

        pipeline = device.CreateRenderPipeline(&descriptor);
        return true;
    }

    void FrameImpl() override {
        wgpu::SurfaceTexture surfaceTexture;
        surface.GetCurrentTexture(&surfaceTexture);
        dawn::utils::ComboRenderPassDescriptor renderPass({surfaceTexture.texture.CreateView()});

        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        {
            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
            pass.SetPipeline(pipeline);
            pass.SetVertexBuffer(0, vertexBuffer);
            pass.SetIndexBuffer(indexBuffer, wgpu::IndexFormat::Uint16);
            pass.DrawIndexed(primitveCount);
            pass.End();
        }

        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);
    }

    wgpu::Buffer vertexBuffer;
    wgpu::Buffer indexBuffer;
    wgpu::RenderPipeline pipeline;
};

int main(int argc, const char* argv[]) {
    
    
    if (!InitSample(argc, argv)) {
        return 1;
    }


    HelloMeshSample* sample = new HelloMeshSample();
    sample->Run(16000);
}
