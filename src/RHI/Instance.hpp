#pragma once

#include "VkTypes.hpp"

namespace RHI {

    class Instance
    {
    public:
        Instance();
        ~Instance();

        Instance(const Instance&) = delete;
        Instance& operator=(const Instance&) = delete;

        Instance(Instance&&) = default;
        Instance& operator=(Instance&&) = default;

        inline VkInstance GetInstance() const { return m_Instance; }

    private:
        VkInstance m_Instance { VK_NULL_HANDLE };
        VkDebugUtilsMessengerEXT m_DebugMessenger { VK_NULL_HANDLE };
    };

}
