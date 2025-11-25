#include "SceneLoader.hpp"

#include <tiny_gltf.h>
#include <glm/gtc/type_ptr.hpp>

#include "Logger.hpp"

namespace Scene {

    std::optional<Scene::SceneData> GlTFLoader::Load(const std::filesystem::path& path)
    {
        tinygltf::Model model;
        tinygltf::TinyGLTF loader;

        std::string err;
        std::string warn;

        if (!path.has_extension()) {
            LOG_ERROR("File does not have an extension: {}", path.string());
            return std::nullopt;
        }

        if (path.extension() != ".glb" && path.extension() != ".gltf") {
            LOG_ERROR("Unsupported file extension: {}", path.extension().string());
            return std::nullopt;
        }

        LOG_INFO("Loading glTF file: {}", path.string());

        bool ret = false;
        if (path.extension() == ".glb") {
            ret = loader.LoadBinaryFromFile(&model, &err, &warn, path.string());
        } else if (path.extension() == ".gltf") {
            ret = loader.LoadASCIIFromFile(&model, &err, &warn, path.string());
        }

        if (!warn.empty()) {
            LOG_WARN("glTF Warning: {}", warn);
        }

        if (!err.empty()) {
            LOG_ERROR("glTF Error: {}", err);
        }

        if (!ret) {
            LOG_ERROR("Failed to load glTF file: {}", path.string());
            return std::nullopt;
        }

        LOG_INFO("Processing glTF model");
        Scene::SceneData data;

        LoadTextures(model, data);
        LOG_INFO("Loaded {} textures", data.textures.size());

        LoadMaterials(model, data);
        LOG_INFO("Loaded {} materials", data.materials.size());

        if (data.materials.empty()) {
            data.materials.emplace_back();
        }

        LoadMeshes(model, data);
        LOG_INFO("Loaded {} unique meshes", data.meshes.size());

        const tinygltf::Scene& scene = model.scenes[model.defaultScene < 0 ? 0 : model.defaultScene];
        for (i32 idx : scene.nodes) {
            LoadNodes(model, model.nodes[idx], data, glm::mat4(1.0f));
        }

        LOG_INFO("Loaded {}", path.string());
        LOG_INFO(" - {} Vertices", data.vertices.size());
        LOG_INFO(" - {} Indices", data.indices.size());
        LOG_INFO(" - {} Instances", data.nodes.size());

        return data;
    }

    glm::mat4 GlTFLoader::GetNodeTransform(const tinygltf::Node& node)
    {
        if (!node.matrix.empty()) {
            return glm::make_mat4(node.matrix.data());
        }

        glm::mat4 T = glm::mat4(1.0f);

        if (!node.translation.empty()) {
            T = glm::translate(T, glm::vec3(node.translation[0], node.translation[1], node.translation[2]));
        }

        if (!node.rotation.empty()) {
            glm::quat R = glm::make_quat(node.rotation.data());
            T = T * glm::mat4_cast(R);
        }

        if (!node.scale.empty()) {
            T = glm::scale(T, glm::vec3(node.scale[0], node.scale[1], node.scale[2]));
        }

        return T;
    }

    void GlTFLoader::LoadTextures(tinygltf::Model& model, Scene::SceneData& scene)
    {
        for (const auto& texture : model.textures) {
            if (texture.source < 0) continue;

            const auto& image = model.images[texture.source];
            if (image.image.empty()) {
                LOG_WARN("Failed to load texture: {}", image.uri);
                continue;
            }

            auto& data = scene.textures.emplace_back();
            data.width = static_cast<u32>(image.width);
            data.height = static_cast<u32>(image.height);
            data.channels = static_cast<u32>(image.component);
            data.pixels.resize(image.image.size());
            memcpy(data.pixels.data(), image.image.data(), image.image.size());
        }
    }

    void GlTFLoader::LoadMaterials(tinygltf::Model& model, Scene::SceneData& scene)
    {
        for (const auto& material : model.materials) {
            auto& data = scene.materials.emplace_back();
            const auto& pbr = material.pbrMetallicRoughness;

            if (!pbr.baseColorFactor.empty()) {
                data.baseColorFactor = glm::make_vec4(pbr.baseColorFactor.data());
            }

            if (!material.emissiveFactor.empty()) {
                data.emissiveFactor = glm::make_vec3(material.emissiveFactor.data());
            }

            data.metallicFactor = static_cast<f32>(pbr.metallicFactor);
            data.roughnessFactor = static_cast<f32>(pbr.roughnessFactor);
            data.alphaCutoff = static_cast<f32>(material.alphaCutoff);

            if (material.alphaMode == "MASK") data.alphaMode = Scene::MaterialData::AlphaMode::Mask;
            if (material.alphaMode == "BLEND") data.alphaMode = Scene::MaterialData::AlphaMode::Blend;

            data.baseColorTexture = pbr.baseColorTexture.index;
            data.metallicRoughnessTexture = pbr.metallicRoughnessTexture.index;
            data.normalTexture = material.normalTexture.index;
            data.occlusionTexture = material.occlusionTexture.index;
            data.emissiveTexture = material.emissiveTexture.index;
        }
    }

    void GlTFLoader::LoadMeshes(tinygltf::Model& model, SceneData& scene)
    {
        for (const auto& mesh : model.meshes) {
            auto& sceneMesh = scene.meshes.emplace_back();

            for (const auto& primitive : mesh.primitives) {
                u32 vertexOffset = static_cast<u32>(scene.vertices.size());
                u32 indexOffset = static_cast<u32>(scene.indices.size());

                u32 vertexCount = 0;
                u32 indexCount = 0;

                if (primitive.indices >= 0) {
                    const auto& accessor = model.accessors[primitive.indices];
                    const auto& bufferView = model.bufferViews[accessor.bufferView];
                    const auto& buffer = model.buffers[bufferView.buffer];

                    indexCount = static_cast<u32>(accessor.count);
                    const void* pData = &(buffer.data[bufferView.byteOffset + accessor.byteOffset]);

                    scene.indices.reserve(scene.indices.size() + accessor.count);

                    switch (accessor.componentType) {
                        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
                            const auto* buf = static_cast<const u8*>(pData);
                            for (usize i = 0; i < accessor.count; ++i) {
                                scene.indices.push_back(static_cast<u32>(buf[i]) + vertexOffset);
                            }
                        } break;
                        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
                            const auto* buf = static_cast<const u16*>(pData);
                            for (usize i = 0; i < accessor.count; ++i) {
                                scene.indices.push_back(static_cast<u32>(buf[i]) + vertexOffset);
                            }
                        } break;
                        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
                            const auto* buf = static_cast<const u32*>(pData);
                            for (usize i = 0; i < accessor.count; ++i) {
                                scene.indices.push_back(buf[i] + vertexOffset);
                            }
                        } break;
                        default:
                            LOG_ERROR("Unsupported index component type: {}", accessor.componentType);
                            continue;
                    }
                } else {
                    const auto& posAccessor = model.accessors[primitive.attributes.at("POSITION")];
                    vertexCount = static_cast<u32>(posAccessor.count);
                    indexCount = vertexCount;

                    scene.indices.reserve(scene.indices.size() + indexCount);
                    for (u32 i = 0; i < indexCount; ++i) {
                        scene.indices.push_back(vertexOffset + i);
                    }
                }

                if (!primitive.attributes.contains("POSITION")) {
                    LOG_WARN("Primitive does not have position attribute, skipping...");
                    continue;
                }

                const auto& posAccessor = model.accessors[primitive.attributes.at("POSITION")];
                const auto& posBufferView = model.bufferViews[posAccessor.bufferView];
                const auto& posBuffer = model.buffers[posBufferView.buffer];

                vertexCount = static_cast<u32>(posAccessor.count);

                const std::byte* pPosBase = reinterpret_cast<const std::byte*>(posBuffer.data.data() + posBufferView.byteOffset + posAccessor.byteOffset);
                const usize posStride = posBufferView.byteStride > 0 ? posBufferView.byteStride : sizeof(glm::vec3);

                const std::byte* pNormBase = nullptr;
                usize normStride = 0;
                if (primitive.attributes.contains("NORMAL")) {
                    const auto& accessor = model.accessors[primitive.attributes.at("NORMAL")];
                    if (accessor.count == vertexCount) {
                        const auto& bufferView  = model.bufferViews[accessor.bufferView];
                        const auto& buffer = model.buffers[bufferView.buffer];
                        pNormBase = reinterpret_cast<const std::byte*>(buffer.data.data() + bufferView.byteOffset + accessor.byteOffset);
                        normStride = bufferView.byteStride > 0 ? bufferView.byteStride : sizeof(glm::vec3);
                    }
                }

                const std::byte* pUVBase = nullptr;
                usize uvStride = 0;
                if (primitive.attributes.contains("TEXCOORD_0")) {
                    const auto& accessor = model.accessors[primitive.attributes.at("TEXCOORD_0")];
                    if (accessor.count == vertexCount) {
                        const auto& bufferView  = model.bufferViews[accessor.bufferView];
                        const auto& buffer = model.buffers[bufferView.buffer];
                        pUVBase = reinterpret_cast<const std::byte*>(buffer.data.data() + bufferView.byteOffset + accessor.byteOffset);
                        uvStride = bufferView.byteStride > 0 ? bufferView.byteStride : sizeof(glm::vec2);
                    }
                }

                const std::byte* pTanBase = nullptr;
                usize tanStride = 0;
                if (primitive.attributes.contains("TANGENT")) {
                    const auto& accessor = model.accessors[primitive.attributes.at("TANGENT")];
                    if (accessor.count == vertexCount) {
                        const auto& bufferView  = model.bufferViews[accessor.bufferView];
                        const auto& buffer = model.buffers[bufferView.buffer];
                        pTanBase = reinterpret_cast<const std::byte*>(buffer.data.data() + bufferView.byteOffset + accessor.byteOffset);
                        tanStride = bufferView.byteStride > 0 ? bufferView.byteStride : sizeof(glm::vec4);
                    }
                }

                scene.vertices.reserve(scene.vertices.size() + vertexCount);
                for (usize v = 0; v < vertexCount; ++v) {
                    auto& vertex = scene.vertices.emplace_back();

                    memcpy(&vertex.position, pPosBase + v * posStride, sizeof(glm::vec3));

                    if (pNormBase) {
                        memcpy(&vertex.normal, pNormBase + v * normStride, sizeof(glm::vec3));
                    } else {
                        vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
                    }

                    if (pUVBase) {
                        memcpy(&vertex.uv0, pUVBase + v * uvStride, sizeof(glm::vec2));
                    } else {
                        vertex.uv0 = glm::vec2(0.0f);
                    }

                    if (pTanBase) {
                        memcpy(&vertex.tangent, pTanBase + v * tanStride, sizeof(glm::vec4));
                    } else {
                        vertex.tangent = glm::vec4(0.0f);
                    }
                }

                sceneMesh.primitives.push_back(MeshPrimitive {
                    .indexOffset = indexOffset,
                    .indexCount= indexCount,
                    .vertexOffset = vertexOffset,
                    .materialIndex = static_cast<u32>(std::max(0, primitive.material))
                });
            }
        }
    }

    void GlTFLoader::LoadNodes(const tinygltf::Model& model, const tinygltf::Node& node, SceneData& scene, const glm::mat4& parentTransform)
    {
        glm::mat4 local = GetNodeTransform(node);
        glm::mat4 global = parentTransform * local;

        if (node.mesh >= 0) {
            scene.nodes.push_back(Node {
                .transform = global,
                .meshIndex = static_cast<u32>(node.mesh)
            });
        }

        for (i32 idx : node.children) {
            LoadNodes(model, model.nodes[idx], scene, global);
        }
    }

    // void GlTFLoader::ProcessNode(const tinygltf::Model& model, const tinygltf::Node& node, Scene::SceneData& scene, const glm::mat4& parentTransform)
    // {
    //     glm::mat4 localTransform = GetNodeTransform(node);
    //     glm::mat4 globalTransform = parentTransform * localTransform;

    //     if (node.mesh >= 0) {
    //         const auto& mesh = model.meshes[node.mesh];
    //         for (const auto& primitive : mesh.primitives) {
    //             u32 vertexOffset = static_cast<u32>(scene.vertices.size());
    //             u32 indexOffset = static_cast<u32>(scene.indices.size());

    //             u32 indexCount = 0;
    //             u32 vertexCount = 0;

    //             if (primitive.indices >= 0) {
    //                 const auto& accessor = model.accessors[primitive.indices];
    //                 const auto& bufferView = model.bufferViews[accessor.bufferView];
    //                 const auto& buffer = model.buffers[bufferView.buffer];

    //                 indexCount = static_cast<u32>(accessor.count);
    //                 const void* pData = &(buffer.data[bufferView.byteOffset + accessor.byteOffset]);

    //                 scene.indices.reserve(scene.indices.size() + accessor.count);

    //                 switch (accessor.componentType) {
    //                     case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
    //                         const auto* buf = static_cast<const u8*>(pData);
    //                         for (usize i = 0; i < accessor.count; ++i) {
    //                             scene.indices.push_back(static_cast<u32>(buf[i]) + vertexOffset);
    //                         }
    //                     } break;
    //                     case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
    //                         const auto* buf = static_cast<const u16*>(pData);
    //                         for (usize i = 0; i < accessor.count; ++i) {
    //                             scene.indices.push_back(static_cast<u32>(buf[i]) + vertexOffset);
    //                         }
    //                     } break;
    //                     case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
    //                         const auto* buf = static_cast<const u32*>(pData);
    //                         for (usize i = 0; i < accessor.count; ++i) {
    //                             scene.indices.push_back(buf[i] + vertexOffset);
    //                         }
    //                     } break;
    //                     default:
    //                         LOG_ERROR("Unsupported index component type: {}", accessor.componentType);
    //                         continue;
    //                 }
    //             } else {
    //                 const auto& posAccessor = model.accessors[primitive.attributes.at("POSITION")];
    //                 vertexCount = static_cast<u32>(posAccessor.count);
    //                 indexCount = vertexCount;

    //                 scene.indices.reserve(scene.indices.size() + indexCount);
    //                 for (u32 i = 0; i < indexCount; ++i) {
    //                     scene.indices.push_back(vertexOffset + i);
    //                 }
    //             }

    //             if (!primitive.attributes.contains("POSITION")) {
    //                 LOG_WARN("primitive does not have position attribute, skipping...");
    //                 continue;
    //             }

    //             const auto& posAccessor = model.accessors[primitive.attributes.at("POSITION")];
    //             const auto& posBufferView = model.bufferViews[posAccessor.bufferView];
    //             const auto& posBuffer = model.buffers[posBufferView.buffer];

    //             vertexCount = posAccessor.count;

    //             const std::byte* pPosBase = reinterpret_cast<const std::byte*>(posBuffer.data.data()) + posBufferView.byteOffset + posAccessor.byteOffset;
    //             const usize posStride = posBufferView.byteStride > 0 ? posBufferView.byteStride : sizeof(glm::vec3);

    //             const std::byte* pNormBase = nullptr;
    //             usize normStride = 0;
    //             if (primitive.attributes.contains("NORMAL")) {
    //                 const auto& accessor = model.accessors[primitive.attributes.at("NORMAL")];
    //                 const auto& bufferView = model.bufferViews[accessor.bufferView];
    //                 const auto& buffer = model.buffers[bufferView.buffer];

    //                 pNormBase = reinterpret_cast<const std::byte*>(buffer.data.data()) + bufferView.byteOffset + accessor.byteOffset;
    //                 normStride = bufferView.byteStride > 0 ? bufferView.byteStride : sizeof(glm::vec3);
    //             }

    //             const std::byte* pUVBase = nullptr;
    //             usize uvStride = 0;
    //             if (primitive.attributes.contains("TEXCOORD_0")) {
    //                 const auto& accessor = model.accessors[primitive.attributes.at("TEXCOORD_0")];
    //                 const auto& bufferView = model.bufferViews[accessor.bufferView];
    //                 const auto& buffer = model.buffers[bufferView.buffer];

    //                 pUVBase = reinterpret_cast<const std::byte*>(buffer.data.data()) + bufferView.byteOffset + accessor.byteOffset;
    //                 uvStride = bufferView.byteStride > 0 ? bufferView.byteStride : sizeof(glm::vec2);
    //             }

    //             const std::byte* pTanBase = nullptr;
    //             usize tanStride = 0;
    //             if (primitive.attributes.contains("TANGENT")) {
    //                 const auto& accessor = model.accessors[primitive.attributes.at("TANGENT")];
    //                 const auto& bufferView = model.bufferViews[accessor.bufferView];
    //                 const auto& buffer = model.buffers[bufferView.buffer];

    //                 pTanBase = reinterpret_cast<const std::byte*>(buffer.data.data()) + bufferView.byteOffset + accessor.byteOffset;
    //                 tanStride = bufferView.byteStride > 0 ? bufferView.byteStride : sizeof(glm::vec4);
    //             }

    //             scene.vertices.reserve(scene.vertices.size() + vertexCount);
    //             for (usize v = 0; v < vertexCount; ++v) {
    //                 auto& vertex = scene.vertices.emplace_back();

    //                 const f32* pCurrentPos = reinterpret_cast<const f32*>(pPosBase + v * posStride);
    //                 vertex.position = glm::make_vec3(pCurrentPos);

    //                 if (pNormBase) {
    //                     const f32* pCurrentNorm = reinterpret_cast<const f32*>(pNormBase + v * normStride);
    //                     vertex.normal = glm::make_vec3(pCurrentNorm);
    //                 } else {
    //                     vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
    //                 }

    //                 if (pUVBase) {
    //                     const f32* pCurrentUV = reinterpret_cast<const f32*>(pUVBase + v * uvStride);
    //                     vertex.uv0 = glm::make_vec2(pCurrentUV);
    //                 } else {
    //                     vertex.uv0 = glm::vec2(0.0f);
    //                 }

    //                 if (pTanBase) {
    //                     const f32* pCurrentTan = reinterpret_cast<const f32*>(pTanBase + v * tanStride);
    //                     vertex.tangent = glm::make_vec4(pCurrentTan);
    //                 } else {
    //                     vertex.tangent = glm::vec4(0.0f);
    //                 }
    //             }

    //             auto& prim = scene.primitives.emplace_back();
    //             prim.indexOffset = indexOffset;
    //             prim.indexCount = indexCount;
    //             prim.vertexOffset = vertexOffset;
    //             prim.materialIndex = std::max(0, primitive.material);

    //             auto& obj = scene.renderables.emplace_back();
    //             obj.transform = globalTransform;
    //             obj.primitiveIndex = static_cast<u32>(scene.primitives.size() - 1);
    //         }
    //     }

    //     for (i32 idx : node.children) {
    //         ProcessNode(model, model.nodes[idx], scene, globalTransform);
    //     }
    // }

}