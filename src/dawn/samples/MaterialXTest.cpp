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

#include <iostream>
#include <string>
#include <vector>

#include <MaterialXCore/Document.h>
#include <MaterialXFormat/XmlIo.h>

namespace mx = MaterialX;

void PrintMaterialInfo(mx::DocumentPtr doc) {
    std::cout << "\n=== MaterialX Document Info ===" << std::endl;
    std::cout << "Version: " << doc->getVersionString() << std::endl;
    
    // List all material nodes
    std::vector<mx::NodePtr> materialNodes = doc->getMaterialNodes();
    std::cout << "\nMaterial Nodes (" << materialNodes.size() << "):" << std::endl;
    for (const auto& material : materialNodes) {
        std::cout << "  - " << material->getName() 
                  << " (category: " << material->getCategory() << ")" << std::endl;
        
        // Print input connections
        std::vector<mx::InputPtr> inputs = material->getInputs();
        for (const auto& input : inputs) {
            std::cout << "    Input: " << input->getName() 
                      << " (type: " << input->getType() << ")";
            if (input->hasNodeName()) {
                std::cout << " <- " << input->getNodeName();
            }
            std::cout << std::endl;
        }
    }
    
    // List all node graphs
    std::vector<mx::NodeGraphPtr> nodeGraphs = doc->getNodeGraphs();
    std::cout << "\nNode Graphs (" << nodeGraphs.size() << "):" << std::endl;
    for (const auto& graph : nodeGraphs) {
        std::cout << "  - " << graph->getName() << std::endl;
        
        // Print nodes in the graph
        std::vector<mx::NodePtr> nodes = graph->getNodes();
        for (const auto& node : nodes) {
            std::cout << "    Node: " << node->getName() 
                      << " (category: " << node->getCategory() << ")" << std::endl;
        }
    }
    
    // List all shader definitions
    std::vector<mx::NodeDefPtr> nodeDefs = doc->getNodeDefs();
    std::cout << "\nNode Definitions (" << nodeDefs.size() << "):" << std::endl;
    for (const auto& nodeDef : nodeDefs) {
        std::cout << "  - " << nodeDef->getName() 
                  << " (node: " << nodeDef->getNodeString() << ")" << std::endl;
    }
    
    // List all looks
    std::vector<mx::LookPtr> looks = doc->getLooks();
    std::cout << "\nLooks (" << looks.size() << "):" << std::endl;
    for (const auto& look : looks) {
        std::cout << "  - " << look->getName() << std::endl;
    }
}

void CreateSampleDocument(const std::string& outputPath) {
    std::cout << "\n=== Creating Sample MaterialX Document ===" << std::endl;
    
    mx::DocumentPtr doc = mx::createDocument();
    doc->setVersionString("1.39");
    
    // Create a node graph with a simple shader
    mx::NodeGraphPtr nodeGraph = doc->addNodeGraph("NG_red_shader");
    
    // Add a constant color node
    mx::NodePtr colorNode = nodeGraph->addNode("constant", "color_red", "color3");
    mx::InputPtr colorInput = colorNode->addInput("value", "color3");
    colorInput->setValue(mx::Color3(0.8f, 0.2f, 0.2f));
    
    // Add output
    mx::OutputPtr graphOutput = nodeGraph->addOutput("surface_out", "color3");
    graphOutput->setConnectedNode(colorNode);
    
    // Create a surface material node that references the shader
    mx::NodePtr surfaceMaterial = doc->addMaterialNode("RedMaterial");
    
    // Create a look and assign the material
    mx::LookPtr look = doc->addLook("DefaultLook");
    mx::MaterialAssignPtr matAssign = look->addMaterialAssign("MA_red", "/*");
    matAssign->setMaterial("RedMaterial");
    
    // Write to file
    mx::writeToXmlFile(doc, outputPath);
    std::cout << "Sample document created: " << outputPath << std::endl;
}

int main(int argc, const char* argv[]) {
    std::cout << "MaterialX Sample Application" << std::endl;
    std::cout << "=============================" << std::endl;
    
    try {
        // Check if a file path was provided
        if (argc < 2) {
            std::cout << "\nNo MaterialX file specified. Creating a sample document..." << std::endl;
            std::string outputPath = "sample_material.mtlx";
            CreateSampleDocument(outputPath);
            
            // Load and display the created document
            mx::DocumentPtr doc = mx::createDocument();
            mx::readFromXmlFile(doc, outputPath);
            PrintMaterialInfo(doc);
            
            std::cout << "\nUsage: " << argv[0] << " <path_to_mtlx_file>" << std::endl;
            return 0;
        }
        
        // Load MaterialX document
        std::string mtlxPath = argv[1];
        std::cout << "\nLoading MaterialX file: " << mtlxPath << std::endl;
        
        mx::DocumentPtr doc = mx::createDocument();
        mx::readFromXmlFile(doc, mtlxPath);
        
        // Validate the document
        std::string validationErrors;
        bool isValid = doc->validate(&validationErrors);
        if (!isValid) {
            std::cerr << "\nValidation errors found:" << std::endl;
            std::cerr << validationErrors << std::endl;
        } else {
            std::cout << "\nDocument validated successfully!" << std::endl;
        }
        
        // Print information about the document
        PrintMaterialInfo(doc);
        
        std::cout << "\n=== MaterialX Loading Complete ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

