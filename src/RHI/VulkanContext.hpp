#pragma once

#include "VulkanTypes.hpp"
#include "Core/Window.hpp"

namespace PathTracer {

    class VulkanContext
    {
    public:
        VulkanContext(std::shared_ptr<Window> window);
        ~VulkanContext();

        inline const VkInstance& GetInstance() { return s_Instance; }
        inline static const VkInstance& GetStaticInstance() { return s_Instance; }

        inline VkSurfaceKHR GetSurface() const { return m_Surface; }

    private:
        inline static std::atomic<usize> s_InstanceCount { 0 };
        inline static VkInstance s_Instance { VK_NULL_HANDLE };

    private:
        std::shared_ptr<Window> m_Window;

        VkSurfaceKHR m_Surface { VK_NULL_HANDLE };
    };

}
