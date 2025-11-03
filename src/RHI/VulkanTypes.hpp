#pragma once

#include <volk.h>

#include "Types.hpp"

#ifndef NDEBUG
    #define VK_CHECK(expr) do { VkResult result_ = (expr); if (result_ != VK_SUCCESS) { std::println(std::cerr, "VK_CHECK Failed: {}", #expr); std::abort(); } } while (false)
#else
    #define VK_CHECK(expr) (expr)
#endif

namespace PathTracer {

    inline u32 FindMemoryType(VkPhysicalDevice device, u32 typeFilter, VkMemoryPropertyFlags properties)
    {
        VkPhysicalDeviceMemoryProperties memoryProperties;
        vkGetPhysicalDeviceMemoryProperties(device, &memoryProperties);

        for (u32 i = 0; i < memoryProperties.memoryTypeCount; ++i) {
            if ((typeFilter & (1u << i)) && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        std::println(std::cerr, "Failed to find suitable memory type");
        return 0;
    }

}
