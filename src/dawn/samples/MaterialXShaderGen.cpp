// Copyright 2025 The Dawn & Tint Authors
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

//
// MaterialXShaderGen Sample
//
// This sample demonstrates the shader generation pipeline:
// 1. Use MaterialX WgslShaderGenerator to generate Vulkan GLSL shaders
// 2. Use glslang to compile GLSL to SPIRV
// 3. Use Tint to convert SPIRV to WGSL
// 4. Use Dawn to compile and validate the WGSL shaders
//

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <fstream>
#include <iomanip>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <MaterialXCore/Document.h>
#include <MaterialXFormat/Util.h>
#include <MaterialXFormat/XmlIo.h>
#include <MaterialXGenShader/GenContext.h>
#include <MaterialXGenShader/Shader.h>
#include <MaterialXGenShader/Util.h>
#include <MaterialXGenGlsl/WgslShaderGenerator.h>

#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <SPIRV/GlslangToSpv.h>

#include "dawn/samples/SampleUtils.h"
#include "dawn/utils/WGPUHelpers.h"

#include "tint/tint.h"
#include "src/tint/lang/wgsl/inspector/inspector.h"
#include "src/tint/lang/wgsl/reader/reader.h"
#include "src/tint/utils/diagnostic/source.h"
#include "src/tint/utils/diagnostic/formatter.h"
#include "src/tint/utils/text/styled_text_printer.h"
#include "src/tint/cmd/common/helper.h"
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <iomanip>

#ifdef _WIN32
#include <direct.h>  // for _mkdir on Windows
#include <io.h>      // for _access on Windows
#else
#include <sys/stat.h>  // for mkdir on Unix
#include <unistd.h>    // for access on Unix
#endif

// MaterialX includes
#include <MaterialXCore/Document.h>
#include <MaterialXFormat/Util.h>
#include <MaterialXFormat/XmlIo.h>
#include <MaterialXGenShader/GenContext.h>
#include <MaterialXGenShader/Shader.h>
#include <MaterialXGenShader/Util.h>
#include <MaterialXGenGlsl/WgslShaderGenerator.h>

// glslang includes for GLSL to SPIRV
#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <SPIRV/GlslangToSpv.h>

// Dawn includes (include before Tint to avoid conflicts)
#include "dawn/samples/SampleUtils.h"
#include "dawn/utils/WGPUHelpers.h"

// Tint API for SPIRV to WGSL
#include "tint/tint.h"
#include "src/tint/lang/wgsl/inspector/inspector.h"
#include "src/tint/lang/wgsl/reader/reader.h"
#include "src/tint/utils/diagnostic/source.h"
#include "src/tint/utils/diagnostic/formatter.h"
#include "src/tint/utils/text/styled_text_printer.h"
#include "src/tint/cmd/common/helper.h"

namespace mx = MaterialX;

//------------------------------------------------------------------------------
// Helper: Convert EShLanguage to string for logging
//------------------------------------------------------------------------------
const char* ShaderStageToString(EShLanguage stage) {
    switch (stage) {
        case EShLangVertex: return "Vertex";
        case EShLangFragment: return "Fragment";
        case EShLangCompute: return "Compute";
        default: return "Unknown";
    }
}

//------------------------------------------------------------------------------
// Helper: Create directory if it doesn't exist (recursive)
//------------------------------------------------------------------------------
bool CreateDirectoryIfNeeded(const std::string& path) {
    if (path.empty()) {
        return true;  // Empty path means no output needed
    }
    
    // Normalize path separators for current platform
    std::string normalizedPath = path;
#ifdef _WIN32
    // Replace forward slashes with backslashes on Windows
    for (size_t i = 0; i < normalizedPath.length(); ++i) {
        if (normalizedPath[i] == '/') {
            normalizedPath[i] = '\\';
        }
    }
    
    // Check if directory exists
    if (_access(normalizedPath.c_str(), 0) == 0) {
        return true;  // Directory already exists
    }
    
    // Create parent directories first
    size_t pos = normalizedPath.find_last_of("\\");
    if (pos != std::string::npos && pos > 0) {
        std::string parent = normalizedPath.substr(0, pos);
        if (!CreateDirectoryIfNeeded(parent)) {
            return false;
        }
    }
    
    // Create the directory
    return _mkdir(normalizedPath.c_str()) == 0;
#else
    // Unix/Linux: use mkdir with mode 0755
    struct stat info;
    if (stat(normalizedPath.c_str(), &info) == 0) {
        return S_ISDIR(info.st_mode);  // Already exists and is a directory
    }
    
    // Create parent directories first
    size_t pos = normalizedPath.find_last_of("/");
    if (pos != std::string::npos && pos > 0) {
        std::string parent = normalizedPath.substr(0, pos);
        if (!CreateDirectoryIfNeeded(parent)) {
            return false;
        }
    }
    
    // Create the directory
    return mkdir(normalizedPath.c_str(), 0755) == 0;
#endif
}

//------------------------------------------------------------------------------
// Helper: Save shader code to file
//------------------------------------------------------------------------------
bool SaveShaderToFile(const std::string& filePath, const std::string& shaderCode) {
    if (filePath.empty()) {
        return false;
    }
    
    std::ofstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "  Error: Failed to open file for writing: " << filePath << std::endl;
        return false;
    }
    
    file << shaderCode;
    file.close();
    
    if (!file.good()) {
        std::cerr << "  Error: Failed to write to file: " << filePath << std::endl;
        return false;
    }
    
    return true;
}

//------------------------------------------------------------------------------
// Helper: Escape JSON string
//------------------------------------------------------------------------------
std::string EscapeJsonString(const std::string& str) {
    std::ostringstream escaped;
    for (char c : str) {
        switch (c) {
            case '"':  escaped << "\\\""; break;
            case '\\': escaped << "\\\\"; break;
            case '\b': escaped << "\\b"; break;
            case '\f': escaped << "\\f"; break;
            case '\n': escaped << "\\n"; break;
            case '\r': escaped << "\\r"; break;
            case '\t': escaped << "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    escaped << "\\u" << std::hex << std::setw(4) << std::setfill('0') 
                            << static_cast<int>(c);
                } else {
                    escaped << c;
                }
                break;
        }
    }
    return escaped.str();
}

//------------------------------------------------------------------------------
// Helper: Convert ResourceBinding to JSON string
//------------------------------------------------------------------------------
std::string ResourceBindingToJson(const tint::inspector::ResourceBinding& binding) {
    std::ostringstream json;
    json << "{\n";
    json << "  \"binding\": " << binding.binding << ",\n";
    json << "  \"group\": " << binding.bind_group << ",\n";
    json << "  \"resource_type\": \"" << tint::cmd::ResourceTypeToString(binding.resource_type) << "\",\n";
    json << "  \"size\": " << binding.size << ",\n";
    json << "  \"size_no_padding\": " << binding.size_no_padding << ",\n";
    
    if (binding.array_size.has_value()) {
        json << "  \"array_size\": " << binding.array_size.value() << ",\n";
    }
    
    json << "  \"dimension\": \"" << tint::cmd::TextureDimensionToString(binding.dim) << "\",\n";
    json << "  \"sampled_kind\": \"" << tint::cmd::SampledKindToString(binding.sampled_kind) << "\",\n";
    json << "  \"image_format\": \"" << tint::cmd::TexelFormatToString(binding.image_format) << "\"";
    
    if (!binding.variable_name.empty()) {
        json << ",\n  \"variable_name\": \"" << EscapeJsonString(binding.variable_name) << "\"";
    }
    
    json << "\n}";
    
    return json.str();
}

//------------------------------------------------------------------------------
// Helper: Extract and save binding information to JSON
//------------------------------------------------------------------------------
bool SaveBindingsToJson(const std::string& wgslCode, const std::string& outputPath) {
    try {
        // Parse WGSL code into a Program
        tint::Source::File source_file("shader.wgsl", wgslCode);
        tint::wgsl::reader::Options options;
        tint::Program program = tint::wgsl::reader::Parse(&source_file, options);
        
        if (program.Diagnostics().ContainsErrors()) {
            std::cerr << "  Error: Failed to parse WGSL for binding extraction:" << std::endl;
            tint::diag::Formatter formatter;
            auto formatted = formatter.Format(program.Diagnostics());
            // Print formatted diagnostics to stderr
            tint::StyledTextPrinter::Create(stderr)->Print(formatted);
            return false;
        }
        
        // Create inspector
        tint::inspector::Inspector inspector(program);
        if (inspector.has_error()) {
            std::cerr << "  Error: Inspector error: " << inspector.error() << std::endl;
            return false;
        }
        
        // Get entry points
        auto entryPoints = inspector.GetEntryPoints();
        if (entryPoints.empty()) {
            std::cerr << "  Warning: No entry points found in shader" << std::endl;
            return false;
        }
        
        // Build JSON
        std::ostringstream json;
        json << "{\n";
        json << "  \"entry_points\": [\n";
        
        bool firstEntryPoint = true;
        for (const auto& entryPoint : entryPoints) {
            if (!firstEntryPoint) {
                json << ",\n";
            }
            firstEntryPoint = false;
            
            json << "    {\n";
            json << "      \"name\": \"" << EscapeJsonString(entryPoint.name) << "\",\n";
            json << "      \"stage\": \"" << tint::cmd::EntryPointStageToString(entryPoint.stage) << "\",\n";
            
            // Get bindings for this entry point
            auto bindings = inspector.GetResourceBindings(entryPoint.name);
            if (inspector.has_error()) {
                std::cerr << "  Warning: Error getting bindings for " << entryPoint.name 
                          << ": " << inspector.error() << std::endl;
            }
            
            json << "      \"bindings\": [\n";
            bool firstBinding = true;
            for (const auto& binding : bindings) {
                if (!firstBinding) {
                    json << ",\n";
                }
                firstBinding = false;
                
                // Indent the binding JSON
                std::string bindingJson = ResourceBindingToJson(binding);
                std::istringstream bindingStream(bindingJson);
                std::string line;
                bool firstLine = true;
                while (std::getline(bindingStream, line)) {
                    if (!firstLine) {
                        json << "\n";
                    }
                    firstLine = false;
                    json << "        " << line;
                }
            }
            json << "\n      ]\n";
            json << "    }";
        }
        
        json << "\n  ]\n";
        json << "}\n";
        
        // Save to file
        return SaveShaderToFile(outputPath, json.str());
        
    } catch (const std::exception& e) {
        std::cerr << "  Error: Exception while extracting bindings: " << e.what() << std::endl;
        return false;
    }
}

//------------------------------------------------------------------------------
// Step 2: Compile GLSL to SPIRV using glslang
//------------------------------------------------------------------------------
bool CompileGlslToSpirv(const std::string& glslSource,
                        EShLanguage stage,
                        std::vector<uint32_t>& spirvOutput,
                        std::string& errorLog) {
    // Initialize glslang (should be done once globally, but safe to call multiple times)
    static bool glslangInitialized = false;
    if (!glslangInitialized) {
        glslang::InitializeProcess();
        glslangInitialized = true;
    }

    const char* shaderStrings[1] = { glslSource.c_str() };
    int shaderLengths[1] = { static_cast<int>(glslSource.length()) };

    glslang::TShader shader(stage);
    shader.setStringsWithLengths(shaderStrings, shaderLengths, 1);
    
    // Set up for Vulkan GLSL (SPIRV target)
    shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, 100);
    shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_0);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);
    shader.setEntryPoint("main");

    // Parse the shader
    EShMessages messages = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules);
    if (!shader.parse(GetDefaultResources(), 450, false, messages)) {
        errorLog = "GLSL parsing failed:\n";
        errorLog += shader.getInfoLog();
        errorLog += "\n";
        errorLog += shader.getInfoDebugLog();
        return false;
    }

    // Link into a program
    glslang::TProgram program;
    program.addShader(&shader);
    
    if (!program.link(messages)) {
        errorLog = "GLSL linking failed:\n";
        errorLog += program.getInfoLog();
        errorLog += "\n";
        errorLog += program.getInfoDebugLog();
        return false;
    }

    // Convert to SPIRV
    glslang::SpvOptions spvOptions;
    spvOptions.generateDebugInfo = false;
    spvOptions.disableOptimizer = false;
    spvOptions.optimizeSize = false;

    glslang::GlslangToSpv(*program.getIntermediate(stage), spirvOutput, &spvOptions);
    
    if (spirvOutput.empty()) {
        errorLog = "SPIRV generation produced no output";
        return false;
    }

    return true;
}

//------------------------------------------------------------------------------
// Step 3: Convert SPIRV to WGSL using Tint
//------------------------------------------------------------------------------
bool ConvertSpirvToWgsl(const std::vector<uint32_t>& spirv,
                        std::string& wgslOutput,
                        std::string& errorLog) {
    tint::wgsl::writer::Options options;
    auto result = tint::SpirvToWgsl(spirv, options);
    
    if (result != tint::Success) {
        errorLog = "SPIRV to WGSL conversion failed:\n";
        errorLog += result.Failure().reason;
        return false;
    }
    
    wgslOutput = result.Get();
    return true;
}

//------------------------------------------------------------------------------
// Step 4: Compile WGSL using Dawn (validation only, no device needed for basic check)
//------------------------------------------------------------------------------
bool ValidateWgslWithDawn(wgpu::Device& device,
                          const std::string& wgslSource,
                          const std::string& label,
                          std::string& errorLog) {
    // Create shader module using Dawn utility function - will trigger validation
    wgpu::ShaderModule module = dawn::utils::CreateShaderModule(device, wgslSource.c_str());
    
    if (!module) {
        errorLog = "Dawn shader module creation failed for: " + label;
        return false;
    }

    // Get compilation info to check for errors
    bool success = true;
    bool done = false;
    
    module.GetCompilationInfo(
        wgpu::CallbackMode::AllowSpontaneous,
        [&success, &done, &errorLog, &label](wgpu::CompilationInfoRequestStatus status,
                                              wgpu::CompilationInfo const* info) {
            done = true;
            if (status != wgpu::CompilationInfoRequestStatus::Success) {
                success = false;
                errorLog = "Failed to get compilation info for: " + label;
                return;
            }
            
            // Check for errors in compilation messages
            for (size_t i = 0; i < info->messageCount; ++i) {
                const auto& msg = info->messages[i];
                if (msg.type == wgpu::CompilationMessageType::Error) {
                    success = false;
                    errorLog += "Shader compilation error in " + label + ":\n";
                    errorLog += "  Line " + std::to_string(msg.lineNum) + 
                               ", Col " + std::to_string(msg.linePos) + ": ";
                    errorLog += std::string(msg.message.data, msg.message.length) + "\n";
                } else if (msg.type == wgpu::CompilationMessageType::Warning) {
                    std::cout << "  Warning in " << label << " (line " << msg.lineNum << "): "
                              << std::string(msg.message.data, msg.message.length) << std::endl;
                }
            }
        });
    
    // Process events until compilation info callback is done
    while (!done) {
        device.GetAdapter().GetInstance().ProcessEvents();
    }
    
    return success;
}

//------------------------------------------------------------------------------
// Create a sample MaterialX document with a simple surface shader
//------------------------------------------------------------------------------
mx::DocumentPtr CreateSampleMaterialXDocument() {
    mx::DocumentPtr doc = mx::createDocument();
    
    // Create a simple node graph with an unlit surface shader
    // This generates a simple shader that outputs a constant color
    mx::NodeGraphPtr nodeGraph = doc->addNodeGraph("NG_simple_shader");
    
    // Add a constant color node
    mx::NodePtr colorNode = nodeGraph->addNode("constant", "base_color", "color3");
    mx::InputPtr colorInput = colorNode->addInput("value", "color3");
    colorInput->setValue(mx::Color3(0.8f, 0.2f, 0.3f));  // Reddish color
    
    // Add an output
    mx::OutputPtr surfaceOutput = nodeGraph->addOutput("out", "color3");
    surfaceOutput->setConnectedNode(colorNode);
    
    return doc;
}

//------------------------------------------------------------------------------
// Main shader generation pipeline
//------------------------------------------------------------------------------
class MaterialXShaderGenSample {
public:
    bool Run(const std::string& libraryPath, const std::string& materialXFile = "", const std::string& outputFolder = "shader_output") {
        std::cout << "=== MaterialX Shader Generation Pipeline ===" << std::endl;
        std::cout << std::endl;

        // Initialize Tint
        tint::Initialize();

        // Create output directory if specified
        if (!outputFolder.empty()) {
            std::cout << "Creating output directory: " << outputFolder << std::endl;
            if (!CreateDirectoryIfNeeded(outputFolder)) {
                std::cerr << "Warning: Failed to create output directory: " << outputFolder << std::endl;
                std::cerr << "  Shaders will not be saved to disk." << std::endl;
            } else {
                std::cout << "  Output directory ready" << std::endl;
            }
        }

        bool success = true;
        
        try {
            // Step 1: Set up MaterialX and generate GLSL
            std::cout << "Step 1: Generating GLSL from MaterialX..." << std::endl;
            
            // Create shader generator for WGSL-compatible Vulkan GLSL
            mx::ShaderGeneratorPtr shaderGen = mx::WgslShaderGenerator::create();
            mx::GenContext context(shaderGen);
            
            // Load MaterialX standard libraries
            // Use default search path if libraryPath is empty, otherwise use provided path
            mx::FileSearchPath searchPath;
            if (libraryPath.empty()) {
                searchPath = mx::getDefaultDataSearchPath();
            } else {
                searchPath = mx::FileSearchPath(libraryPath);
            }
            
            mx::DocumentPtr stdLib = mx::createDocument();
            mx::FilePathVec libFolders = { mx::FilePath("libraries") };
            mx::loadLibraries(libFolders, searchPath, stdLib);
            
            if (stdLib->getNodeDefs().empty()) {
                std::cerr << "Warning: No node definitions loaded from libraries. Check search path: " 
                          << searchPath.asString() << std::endl;
            }
            
            // Register source code search path
            context.registerSourceCodeSearchPath(searchPath);
            
            // Register shader metadata from the libraries
            shaderGen->registerShaderMetadata(stdLib, context);
            
            // Load MaterialX document - either from file or create sample
            mx::DocumentPtr doc = mx::createDocument();
            
            if (!materialXFile.empty()) {
                // Load MaterialX file from disk
                std::cout << "  Loading MaterialX file: " << materialXFile << std::endl;
                try {
                    mx::readFromXmlFile(doc, mx::FilePath(materialXFile), searchPath);
                    std::cout << "  Successfully loaded MaterialX file" << std::endl;
                } catch (const std::exception& e) {
                    std::cerr << "  Error loading MaterialX file: " << e.what() << std::endl;
                    return false;
                }
            } else {
                // Create sample document
                doc = CreateSampleMaterialXDocument();
            }
            
            // Set the data library (this makes library definitions available to the document)
            doc->setDataLibrary(stdLib);
            
            // Register type definitions from the document (includes library types)
            shaderGen->registerTypeDefs(doc);
            
            // Register shader metadata from the document
            shaderGen->registerShaderMetadata(doc, context);
            
            // Validate document
            std::string validationErrors;
            if (!doc->validate(&validationErrors)) {
                std::cerr << "MaterialX document validation warnings:" << std::endl;
                std::cerr << validationErrors << std::endl;
            }
            
            // Find renderable elements
            std::vector<mx::TypedElementPtr> renderableElements = mx::findRenderableElements(doc);
            
            if (renderableElements.empty()) {
                if (!materialXFile.empty()) {
                    std::cerr << "Error: No renderable elements found in MaterialX file: " << materialXFile << std::endl;
                    std::cerr << "  A MaterialX file should contain a material node or surface shader." << std::endl;
                } else {
                    // If no renderable elements found, use the node graph output directly
                    mx::NodeGraphPtr nodeGraph = doc->getNodeGraph("NG_simple_shader");
                    if (nodeGraph) {
                        mx::OutputPtr output = nodeGraph->getOutput("out");
                        if (output) {
                            renderableElements.push_back(output);
                        }
                    }
                }
                
                if (renderableElements.empty()) {
                    std::cerr << "Error: No renderable elements found in document" << std::endl;
                    return false;
                }
            }

            std::cout << "  Found " << renderableElements.size() << " renderable element(s)" << std::endl;
            
            // Generate shaders for each renderable element
            for (const auto& elem : renderableElements) {
                std::cout << std::endl;
                std::cout << "Processing element: " << elem->getName() << std::endl;
                
                mx::ShaderPtr shader = nullptr;
                try {
                    shader = shaderGen->generate(elem->getName(), elem, context);
                } catch (const std::exception& e) {
                    std::cerr << "  Error generating shader: " << e.what() << std::endl;
                    success = false;
                    continue;
                }
                
                if (!shader) {
                    std::cerr << "  Error: Shader generation returned null" << std::endl;
                    success = false;
                    continue;
                }

                // Process vertex and pixel stages
                struct ShaderStageInfo {
                    std::string stageName;
                    EShLanguage glslStage;
                };
                
                std::vector<ShaderStageInfo> stages = {
                    { mx::Stage::VERTEX, EShLangVertex },
                    { mx::Stage::PIXEL, EShLangFragment }
                };
                
                for (const auto& stageInfo : stages) {
                    const std::string& glslCode = shader->getSourceCode(stageInfo.stageName);
                    
                    if (glslCode.empty()) {
                        std::cout << "  " << stageInfo.stageName << " stage: No code generated (skipping)" << std::endl;
                        continue;
                    }
                    
                    std::cout << "  " << stageInfo.stageName << " stage:" << std::endl;
                    std::cout << "    GLSL code length: " << glslCode.length() << " chars" << std::endl;
                    
                    // Print first few lines of GLSL for debugging
                    std::cout << "    GLSL preview (first 500 chars):" << std::endl;
                    std::string preview = glslCode.substr(0, std::min<size_t>(500, glslCode.length()));
                    std::cout << "    ---" << std::endl;
                    // Print with indentation
                    std::istringstream iss(preview);
                    std::string line;
                    int lineCount = 0;
                    while (std::getline(iss, line) && lineCount < 15) {
                        std::cout << "    " << line << std::endl;
                        lineCount++;
                    }
                    std::cout << "    ..." << std::endl;
                    std::cout << "    ---" << std::endl;
                    
                    // Step 2: Compile GLSL to SPIRV
                    std::cout << "    Compiling GLSL to SPIRV..." << std::endl;
                    std::vector<uint32_t> spirv;
                    std::string errorLog;
                    
                    if (!CompileGlslToSpirv(glslCode, stageInfo.glslStage, spirv, errorLog)) {
                        std::cerr << "    ERROR: GLSL to SPIRV compilation failed:" << std::endl;
                        std::cerr << errorLog << std::endl;
                        success = false;
                        continue;
                    }
                    std::cout << "    SPIRV generated: " << spirv.size() << " words" << std::endl;
                    
                    // Save GLSL shader to file
                    if (!outputFolder.empty()) {
#ifdef _WIN32
                        std::string glslFileName = outputFolder + "\\" + elem->getName() + "_" + stageInfo.stageName + ".glsl";
#else
                        std::string glslFileName = outputFolder + "/" + elem->getName() + "_" + stageInfo.stageName + ".glsl";
#endif
                        if (SaveShaderToFile(glslFileName, glslCode)) {
                            std::cout << "    Saved GLSL shader to: " << glslFileName << std::endl;
                        }
                    }
                    
                    // Step 3: Convert SPIRV to WGSL
                    std::cout << "    Converting SPIRV to WGSL..." << std::endl;
                    std::string wgslCode;
                    
                    if (!ConvertSpirvToWgsl(spirv, wgslCode, errorLog)) {
                        std::cerr << "    ERROR: SPIRV to WGSL conversion failed:" << std::endl;
                        std::cerr << errorLog << std::endl;
                        success = false;
                        continue;
                    }
                    std::cout << "    WGSL code length: " << wgslCode.length() << " chars" << std::endl;
                    
                    // Print WGSL preview
                    std::cout << "    WGSL preview (first 500 chars):" << std::endl;
                    std::cout << "    ---" << std::endl;
                    preview = wgslCode.substr(0, std::min<size_t>(500, wgslCode.length()));
                    std::istringstream wgslIss(preview);
                    lineCount = 0;
                    while (std::getline(wgslIss, line) && lineCount < 15) {
                        std::cout << "    " << line << std::endl;
                        lineCount++;
                    }
                    std::cout << "    ..." << std::endl;
                    std::cout << "    ---" << std::endl;
                    
                    // Save WGSL shader to file
                    if (!outputFolder.empty()) {
#ifdef _WIN32
                        std::string wgslFileName = outputFolder + "\\" + elem->getName() + "_" + stageInfo.stageName + ".wgsl";
#else
                        std::string wgslFileName = outputFolder + "/" + elem->getName() + "_" + stageInfo.stageName + ".wgsl";
#endif
                        if (SaveShaderToFile(wgslFileName, wgslCode)) {
                            std::cout << "    Saved WGSL shader to: " << wgslFileName << std::endl;
                        }
                        
                        // Extract and save binding information to JSON
                        std::string jsonFileName;
#ifdef _WIN32
                        jsonFileName = outputFolder + "\\" + elem->getName() + "_" + stageInfo.stageName + "_bindings.json";
#else
                        jsonFileName = outputFolder + "/" + elem->getName() + "_" + stageInfo.stageName + "_bindings.json";
#endif
                        
                        if (SaveBindingsToJson(wgslCode, jsonFileName)) {
                            std::cout << "    Saved binding information to: " << jsonFileName << std::endl;
                        } else {
                            std::cerr << "    Warning: Failed to extract binding information" << std::endl;
                        }
                    }
                    
                    // Step 4: Validate with Dawn (if we have a device)
                    if (device_) {
                        std::cout << "    Validating WGSL with Dawn..." << std::endl;
                        std::string shaderLabel = elem->getName() + "_" + stageInfo.stageName;
                        
                        if (!ValidateWgslWithDawn(*device_, wgslCode, shaderLabel, errorLog)) {
                            std::cerr << "    ERROR: Dawn WGSL validation failed:" << std::endl;
                            std::cerr << errorLog << std::endl;
                            success = false;
                        } else {
                            std::cout << "    SUCCESS: WGSL shader compiled successfully!" << std::endl;
                        }
                    } else {
                        std::cout << "    (Skipping Dawn validation - no device available)" << std::endl;
                    }
                }
            }
            
        } catch (const std::exception& e) {
            std::cerr << "Exception during shader generation: " << e.what() << std::endl;
            success = false;
        }
        
        // Cleanup
        tint::Shutdown();
        
        std::cout << std::endl;
        std::cout << "=== Pipeline " << (success ? "COMPLETED SUCCESSFULLY" : "COMPLETED WITH ERRORS") << " ===" << std::endl;
        
        return success;
    }
    
    void SetDevice(wgpu::Device* device) {
        device_ = device;
    }

private:
    wgpu::Device* device_ = nullptr;
};

//------------------------------------------------------------------------------
// Simple Dawn initialization for shader validation
//------------------------------------------------------------------------------
class ShaderValidationApp : public SampleBase {
public:
    using SampleBase::SampleBase;
    
    void SetLibraryPath(const std::string& path) {
        libraryPath_ = path;
    }
    
    void SetMaterialXFile(const std::string& file) {
        materialXFile_ = file;
    }
    
    void SetOutputFolder(const std::string& folder) {
        outputFolder_ = folder;
    }
    
private:
    bool SetupImpl() override {
        // Run the shader generation pipeline
        MaterialXShaderGenSample sample;
        sample.SetDevice(&device);
        
        pipelineSuccess_ = sample.Run(libraryPath_, materialXFile_, outputFolder_);
        
        // Signal that we're done - we don't need to keep running
        return false;  // Return false to exit after setup
    }
    
    void FrameImpl() override {
        // Nothing to render - this is a validation-only sample
    }
    
    std::string libraryPath_;
    std::string materialXFile_;
    std::string outputFolder_ = "shader_output";
    bool pipelineSuccess_ = false;
};

//------------------------------------------------------------------------------
// Main
//------------------------------------------------------------------------------
int main(int argc, const char* argv[]) {
    std::cout << "MaterialX Shader Generation Sample" << std::endl;
    std::cout << "===================================" << std::endl;
    std::cout << std::endl;
    
    // Default library path - use empty string to use MaterialX's default search path
    std::string libraryPath = "";
    std::string materialXFile = "";
    std::string outputFolder = "shader_output";
    
    // Check for command-line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.find("--materialx-path=") == 0) {
            libraryPath = arg.substr(17);
        } else if (arg.find("--file=") == 0 || arg.find("-f=") == 0) {
            materialXFile = arg.substr(arg.find("=") + 1);
        } else if (arg.find("--output=") == 0 || arg.find("-o=") == 0) {
            outputFolder = arg.substr(arg.find("=") + 1);
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options] [materialx_file.mtlx]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  --materialx-path=<path>  Path to MaterialX directory" << std::endl;
            std::cout << "  --file=<path>, -f=<path>  MaterialX file to process" << std::endl;
            std::cout << "  --output=<path>, -o=<path> Output folder for shaders (default: shader_output)" << std::endl;
            std::cout << "  --help, -h                Show this help message" << std::endl;
            std::cout << std::endl;
            std::cout << "Examples:" << std::endl;
            std::cout << "  " << argv[0] << " standard_surface_carpaint.mtlx" << std::endl;
            std::cout << "  " << argv[0] << " --file=path/to/material.mtlx" << std::endl;
            std::cout << "  " << argv[0] << " third_party/materialx/resources/Materials/Examples/StandardSurface/standard_surface_carpaint.mtlx" << std::endl;
            std::cout << "  " << argv[0] << " --materialx-path=third_party/materialx --file=resources/Materials/Examples/StandardSurface/standard_surface_carpaint.mtlx" << std::endl;
            return 0;
        } else if (arg[0] != '-') {
            // Treat as MaterialX file path if it doesn't start with -
            materialXFile = arg;
        }
    }
    
    if (libraryPath.empty()) {
        std::cout << "Using MaterialX default search path" << std::endl;
    } else {
        std::cout << "Using MaterialX path: " << libraryPath << std::endl;
    }
    
    if (!materialXFile.empty()) {
        std::cout << "Processing MaterialX file: " << materialXFile << std::endl;
    } else {
        std::cout << "Using built-in sample material" << std::endl;
    }
    std::cout << "Output folder: " << outputFolder << std::endl;
    std::cout << std::endl;
    
    // Try to run with Dawn device for full validation
    if (InitSample(argc, argv)) {
        ShaderValidationApp app;
        app.SetLibraryPath(libraryPath);
        app.SetMaterialXFile(materialXFile);
        app.SetOutputFolder(outputFolder);
        app.Run(0);  // Run with no delay - exits after setup
    } else {
        // If Dawn initialization fails, run without device validation
        std::cout << "Note: Running without Dawn device (WGSL validation skipped)" << std::endl;
        std::cout << std::endl;
        
        tint::Initialize();
        MaterialXShaderGenSample sample;
        bool success = sample.Run(libraryPath, materialXFile, outputFolder);
        tint::Shutdown();
        
        return success ? 0 : 1;
    }
    
    return 0;
}

