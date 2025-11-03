#include "VulkanContext.hpp"

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "Core/Types.hpp"
#include "Core/Logger.hpp"

namespace PathTracer {

    VulkanContext::VulkanContext(std::shared_ptr<Window> window)
        : m_Window(window)
    {
        if (s_InstanceCount.fetch_add(1, std::memory_order_relaxed) == 0) {
            VK_CHECK(volkInitialize());

            u32 version = volkGetInstanceVersion();

            LOG_INFO("Vulkan version: {}.{}.{}", VK_VERSION_MAJOR(version), VK_VERSION_MINOR(version), VK_VERSION_PATCH(version));

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
#ifndef NDEBUG
            instanceLayers.push_back("VK_LAYER_KHRONOS_validation");
#endif

            std::vector<const char*> instanceExtensions = Window::GetRequiredVulkanExtensions();

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

        VK_CHECK(glfwCreateWindowSurface(s_Instance, m_Window->GetNative(), nullptr, &m_Surface));
    }

    VulkanContext::~VulkanContext()
    {
        vkDestroySurfaceKHR(s_Instance, m_Surface, nullptr);

        if (s_InstanceCount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            vkDestroyInstance(s_Instance, nullptr);
            s_Instance = VK_NULL_HANDLE;
        }
    }

}
