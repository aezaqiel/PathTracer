#pragma once

#include <optional>
#include <filesystem>

#include "Scene/SceneData.hpp"

namespace tinygltf {

    class Model;
    class Node;

}

namespace Scene {

    class GlTFLoader
    {
    public:
        static std::optional<Scene::SceneData> Load(const std::filesystem::path& path);

    private:
        static glm::mat4 GetNodeTransform(const tinygltf::Node& node);

        static void LoadTextures(tinygltf::Model& model, Scene::SceneData& scene);
        static void LoadMaterials(tinygltf::Model& model, Scene::SceneData& scene);

        static void LoadMeshes(tinygltf::Model& model, SceneData& scene);
        static void LoadNodes(const tinygltf::Model& model, const tinygltf::Node& node, SceneData& scene, const glm::mat4& parentTransform);

        // static void ProcessNode(const tinygltf::Model& model, const tinygltf::Node& node, Scene::SceneData& scene, const glm::mat4& parentTransform);
    };

}