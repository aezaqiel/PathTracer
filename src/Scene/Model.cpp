#include "Model.hpp"

#include <tiny_gltf.h>

#include "Core/Logger.hpp"
#include "Core/Timer.hpp"

namespace PathTracer {

    Model::Model(const std::string& filename)
    {
        Timer timer;

        std::string filepath = std::format("{}{}", ASSET_DIR, filename);
        LoadGLTF(filepath);

        LOG_INFO("Loaded {} with {} meshes and {} materials in {} seconds", filename, m_Meshes.size(), m_Materials.size(), static_cast<f32>(timer.GetTotalTime()));
    }

    void Model::LoadGLTF(const std::string& filepath)
    {
        tinygltf::Model model;
        tinygltf::TinyGLTF loader;

        std::string err;
        std::string warn;

        std::string ext = filepath.substr(filepath.find_last_of('.') + 1);

        bool ret = false;
        if (ext == "gltf") {
            ret = loader.LoadASCIIFromFile(&model, &err, &warn, filepath);
        } else if (ext == "glb") {
            ret = loader.LoadBinaryFromFile(&model, &err, &warn, filepath);
        } else {
            LOG_ERROR("Unknown file extension {} for Model::LoadGLTF", ext);
        }

        if (!ret) {
            LOG_ERROR("Failed to parse gltf file {}", filepath);
            return;
        }

        if (!warn.empty()) {
            LOG_WARN("Model Loader: {}", warn.c_str());
        }

        if (!err.empty()) {
            LOG_ERROR("Model Loader: {}", err.c_str());
        }

        m_Materials.reserve(model.materials.size());
        for (const auto& src : model.materials) {
            Material mat;

            const auto& pbr = src.pbrMetallicRoughness;
            if (pbr.baseColorFactor.size() == 4) {
                mat.color = {
                    static_cast<f32>(pbr.baseColorFactor[0]),
                    static_cast<f32>(pbr.baseColorFactor[1]),
                    static_cast<f32>(pbr.baseColorFactor[2])
                };
            } else {
                mat.color = glm::vec3(1.0f, 0.0f, 1.0f);
            }

            m_Materials.push_back(mat);
        }

        if (m_Materials.empty()) {
            m_Materials.push_back(Material {
                .color = glm::vec3(0.8f, 0.8f, 0.8f)
            });
        }

        std::function<void(const tinygltf::Node&)> processNode;
        processNode = [&](const tinygltf::Node& node) {
            if (node.mesh > -1) {
                const auto& gltfMesh = model.meshes[node.mesh];

                for (const auto& primitive : gltfMesh.primitives) {
                    if (primitive.indices < 0 || primitive.mode != TINYGLTF_MODE_TRIANGLES) {
                        continue;
                    }

                    Mesh newMesh;

                    if (primitive.material > -1) {
                        newMesh.materialIndex = static_cast<u32>(primitive.material);
                    } else {
                        newMesh.materialIndex = 0;
                    }

                    {
                        const auto& accessor = model.accessors[primitive.indices];
                        const auto& bufferView = model.bufferViews[accessor.bufferView];
                        const auto& buffer = model.buffers[bufferView.buffer];

                        const unsigned char* data = buffer.data.data();

                        newMesh.indices.reserve(accessor.count);

                        switch (accessor.componentType) {
                            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
                                const u8* p = reinterpret_cast<const u8*>(data);
                                for (usize i = 0; i < accessor.count; ++i) {
                                    newMesh.indices.push_back(static_cast<u32>(p[i]));
                                }
                            } break;
                            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
                                const u16* p = reinterpret_cast<const u16*>(data);
                                for (usize i = 0; i < accessor.count; ++i) {
                                    newMesh.indices.push_back(static_cast<u32>(p[i]));
                                }
                            } break;
                            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
                                const u32* p = reinterpret_cast<const u32*>(data);
                                for (usize i = 0; i < accessor.count; ++i) {
                                    newMesh.indices.push_back(p[i]);
                                }
                            } break;
                            default: {
                                LOG_WARN("Unsupported index type. Skipping primitive");
                                continue;
                            } break;
                        }
                    }

                    auto posIt = primitive.attributes.find("POSITION");
                    if (posIt == primitive.attributes.end()) {
                        LOG_ERROR("Primitive has no POSITION attribute, skipping");
                        continue;
                    }

                    {
                        const auto& accessor = model.accessors[posIt->second];
                        const auto& bufferView = model.bufferViews[accessor.bufferView];
                        const auto& buffer = model.buffers[bufferView.buffer];

                        const unsigned char* data = buffer.data.data();

                        if (accessor.type != TINYGLTF_TYPE_VEC3 || accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) {
                            LOG_ERROR("POSITION attribute is not VEC/FLOAT, skipping");
                            continue;
                        }

                        usize vertexCount = accessor.count;
                        newMesh.vertices.resize(vertexCount);

                        usize stride = bufferView.byteStride > 0 ? bufferView.byteStride : sizeof(glm::vec3);

                        for (usize i = 0; i < vertexCount; ++i) {
                            newMesh.vertices[i].position = *reinterpret_cast<const glm::vec3*>(data + i * stride);
                        }
                    }

                    m_Meshes.push_back(std::move(newMesh));
                }
            }

            for (i32 childIndex : node.children) {
                processNode(model.nodes[childIndex]);
            }
        };

        i32 sceneIndex = model.defaultScene > -1 ? model.defaultScene : 0;
        const auto& scene = model.scenes[sceneIndex];

        for (i32 nodeIndex : scene.nodes) {
            processNode(model.nodes[nodeIndex]);
        }
    }

}
