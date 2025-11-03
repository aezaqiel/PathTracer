#pragma once

#include "VulkanTypes.hpp"

namespace PathTracer {

    class VulkanContext
    {
    public:
        VulkanContext();
        ~VulkanContext();

        inline const VkInstance& GetInstance() { return s_Instance; }

    private:
        inline static VkInstance s_Instance { VK_NULL_HANDLE };
    };

}
