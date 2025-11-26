#pragma once

#include "VkTypes.hpp"

class Window;

namespace RHI {

    class Instance
    {
    public:
        Instance(const std::shared_ptr<Window>& window);
        ~Instance();

        Instance(const Instance&) = delete;
        Instance& operator=(const Instance&) = delete;

        Instance(Instance&&) = default;
        Instance& operator=(Instance&&) = default;

        inline VkInstance GetInstance() const { return m_Instance; }
        inline VkSurfaceKHR GetSurface() const { return m_Surface; }

    private:
        std::shared_ptr<Window> m_Window;

        VkInstance m_Instance { VK_NULL_HANDLE };
        VkDebugUtilsMessengerEXT m_DebugMessenger { VK_NULL_HANDLE };
        VkSurfaceKHR m_Surface { VK_NULL_HANDLE };
    };

}
