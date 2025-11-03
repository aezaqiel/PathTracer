#include "VulkanContext.hpp"

#include "Core/Types.hpp"

namespace PathTracer {

    VulkanContext::VulkanContext()
    {
        if (s_Instance != VK_NULL_HANDLE) {
            return;
        }

        VK_CHECK(volkInitialize());

        u32 version = volkGetInstanceVersion();

        std::println("Vulkan version: {}.{}.{}", VK_VERSION_MAJOR(version), VK_VERSION_MINOR(version), VK_VERSION_PATCH(version));

        VkApplicationInfo info {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pNext = nullptr,
            .pApplicationName = "PathTracer",
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName = "No Engine",
            .engineVersion = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion = version
        };

        std::vector<const char*> instanceLayers;
        std::vector<const char*> instanceExtensions;

#ifndef NDEBUG
        instanceLayers.push_back("VK_LAYER_KHRONOS_validation");
#endif

        VkInstanceCreateInfo instanceInfo {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .pApplicationInfo = &info,
            .enabledLayerCount = static_cast<u32>(instanceLayers.size()),
            .ppEnabledLayerNames = instanceLayers.data(),
            .enabledExtensionCount = static_cast<u32>(instanceExtensions.size()),
            .ppEnabledExtensionNames = instanceExtensions.data()
        };

        VK_CHECK(vkCreateInstance(&instanceInfo, nullptr, &s_Instance));
        volkLoadInstance(s_Instance);
    }

    VulkanContext::~VulkanContext()
    {
        vkDestroyInstance(s_Instance, nullptr);
        s_Instance = VK_NULL_HANDLE;
    }

}
