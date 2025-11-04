#include "AssetManager.hpp"

#include <tiny_gltf.h>

#include "Core/Timer.hpp"
#include "Core/Logger.hpp"

namespace PathTracer {

    std::shared_ptr<Texture> AssetManager::GetTexture(const std::string& filename)
    {
        if (m_TextureCache.find(filename) != m_TextureCache.end()) {
            return m_TextureCache[filename];
        }

        auto texture = LoadTextureFromFile(GetFullAssetPath(filename));
        m_TextureCache[filename] = texture;

        return texture;
    }

    std::shared_ptr<Model> AssetManager::GetModel(const std::string& filename)
    {
        if (m_ModelCache.find(filename) != m_ModelCache.end()) {
            return m_ModelCache[filename];
        }

        auto model = LoadModelFromFile(GetFullAssetPath(filename));
        m_ModelCache[filename] = model;

        return model;
    }

    std::shared_ptr<Texture> AssetManager::LoadTextureFromFile(const std::string& filepath)
    {
        return std::make_shared<Texture>(filepath);
    }

    std::shared_ptr<Model> AssetManager::LoadModelFromFile(const std::string& filepath)
    {
        Timer timer;
        auto loadedModel = std::make_shared<Model>();

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
            return nullptr;
        }

        if (!warn.empty()) {
            LOG_WARN("Model Loader: {}", warn.c_str());
        }

        if (!err.empty()) {
            LOG_ERROR("Model Loader: {}", err.c_str());
        }

        loadedModel->materials.reserve(model.materials.size());
        for (const auto& src : model.materials) {
            Material mat;

            const auto& pbr = src.pbrMetallicRoughness;
            if (pbr.baseColorFactor.size() == 4) {
                mat.baseColorFactor = {
                    static_cast<f32>(pbr.baseColorFactor[0]),
                    static_cast<f32>(pbr.baseColorFactor[1]),
                    static_cast<f32>(pbr.baseColorFactor[2])
                };
            }

            loadedModel->materials.push_back(mat);
        }

        if (loadedModel->materials.empty()) {
            loadedModel->materials.push_back(Material{});
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

                    loadedModel->meshes.push_back(std::move(newMesh));
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

        LOG_INFO("Loaded {} with {} meshes, {} materials, and {} textures in {:.2f} seconds", 
            filepath, loadedModel->GetMeshCount(), loadedModel->GetMeshCount(), loadedModel->GetTextureCount(),
            static_cast<f32>(timer.GetTotalTime()));

        return loadedModel;
    }

    std::string AssetManager::GetFullAssetPath(const std::string& filename)
    {
        return std::format("{}{}", ASSET_DIR, filename);
    }

}
