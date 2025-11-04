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

        auto model = LoadModelFromFile(filename);
        m_ModelCache[filename] = model;

        return model;
    }

    std::shared_ptr<Texture> AssetManager::LoadTextureFromFile(const std::string& filepath)
    {
        return std::make_shared<Texture>(filepath);
    }

    std::shared_ptr<Model> AssetManager::LoadModelFromFile(const std::string& filename)
    {
        Timer timer;
        auto loadedModel = std::make_shared<Model>();

        std::string filepath = GetFullAssetPath(filename);

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

        std::string modelRootDir;
        auto lastSlash = filepath.find_last_of('/');
        if (lastSlash != std::string::npos) {
            modelRootDir = filepath.substr(0, lastSlash + 1);
        }

        loadedModel->textures.reserve(model.images.size());
        for (const auto& image : model.images) {
            if (image.uri.empty()) {
                LOG_ERROR("Embedded textures not supported, skipping image");
                continue;
            }

            std::string texFilename = modelRootDir + image.uri;
            loadedModel->textures.push_back(GetTexture(texFilename));
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

            if (src.emissiveFactor.size() == 3) {
                mat.emissiveFactor = {
                    static_cast<f32>(src.emissiveFactor[0]),
                    static_cast<f32>(src.emissiveFactor[1]),
                    static_cast<f32>(src.emissiveFactor[2])
                };
            }

            mat.metallicFactor = static_cast<f32>(pbr.metallicFactor);
            mat.roughnessFactor = static_cast<f32>(pbr.roughnessFactor);

            mat.baseColorTexture = pbr.baseColorTexture.index;
            mat.metallicRoughnessTexture = pbr.metallicRoughnessTexture.index;
            mat.normalTexture = src.normalTexture.index;
            mat.emissiveTexture = src.emissiveTexture.index;
            mat.occlusionTexture = src.occlusionTexture.index;

            loadedModel->materials.push_back(std::move(mat));
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

                    auto normIt = primitive.attributes.find("NORMAL");
                    if (normIt != primitive.attributes.end()) {
                        const auto& accessor = model.accessors[normIt->second];
                        const auto& bufferView = model.bufferViews[accessor.bufferView];
                        const auto& buffer = model.buffers[bufferView.buffer];

                        const unsigned char* data = buffer.data.data();

                        if (accessor.type == TINYGLTF_TYPE_VEC3 && accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
                            usize vertexCount = accessor.count;
                            usize stride = bufferView.byteStride > 0 ? bufferView.byteStride : sizeof(glm::vec3);

                            const unsigned char* buf = data + bufferView.byteOffset + accessor.byteOffset;

                            for (usize i = 0; i < vertexCount; ++i) {
                                newMesh.vertices[i].normal = *reinterpret_cast<const glm::vec3*>(buf + i * stride);
                            }
                        } else {
                            LOG_WARN("NORMAL attribute is not VEC3/FLOAT, skipping");
                        }
                    }

                    auto tcIt = primitive.attributes.find("TEXCOORD_0");
                    if (tcIt != primitive.attributes.end()) {
                        const auto& accessor = model.accessors[tcIt->second];
                        const auto& bufferView = model.bufferViews[accessor.bufferView];
                        const auto& buffer = model.buffers[bufferView.buffer];

                        const unsigned char* data = buffer.data.data();

                        if (accessor.type == TINYGLTF_TYPE_VEC2 && accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
                            usize vertexCount = accessor.count;
                            usize stride = bufferView.byteStride > 0 ? bufferView.byteStride : sizeof(glm::vec2);

                            const unsigned char* buf = data + bufferView.byteOffset + accessor.byteOffset;

                            for (usize i = 0; i < vertexCount; ++i) {
                                newMesh.vertices[i].texCoord = *reinterpret_cast<const glm::vec2*>(buf + i * stride);
                            }
                        } else {
                            LOG_WARN("TEXCOORD_0 attribute is not VEC2/FLOAT, skipping");
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
