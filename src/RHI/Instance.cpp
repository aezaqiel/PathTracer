#include "Instance.hpp"

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "Core/Window.hpp"

namespace RHI {

    namespace {

        VkBool32 DebugMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT* data, void* userData)
        {
            std::stringstream ss;

            if (type & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) { ss << "[GENERAL]"; }
            if (type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) { ss << "[VALIDATION]"; }
            if (type & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) { ss << "[PERFORMANCE]"; }
            if (type & VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT) { ss << "[ADDRESS]"; }

            if (data->pMessageIdName) {
                ss << " (" << data->pMessageIdName << ")";
            }

            ss << ":\n";
            ss << "  Message: " << data->pMessage << "\n";

            if (data->objectCount > 0) {
                ss << "  Objects (" << data->objectCount << "):\n";

                for (u32 i = 0; i < data->objectCount; ++i) {
                    const auto& obj = data->pObjects[i];

                    ss << "    - Object " << i << ": ";

                    if (obj.pObjectName) {
                        ss << "Name = \"" << obj.pObjectName << "\"";
                    } else {
                        ss << "Handle = " << reinterpret_cast<void*>(obj.objectHandle);
                    }

                    ss << ", Type = " << obj.objectType << "\n";
                }
            }

            if (data->cmdBufLabelCount > 0) {
                ss << "  Command Buffer Labels (" << data->cmdBufLabelCount << "):\n";
                for (u32 i = 0; i < data->cmdBufLabelCount; ++i) {
                    ss << "    - " << data->pCmdBufLabels[i].pLabelName << "\n";
                }
            }

            if (data->queueLabelCount > 0) {
                ss << "  Queue Labels (" << data->queueLabelCount << "):\n";
                for (u32 i = 0; i < data->queueLabelCount; ++i) {
                    ss << "    - " << data->pQueueLabels[i].pLabelName << "\n";
                }
            }

            switch (severity) {
                case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: {
                    LOG_TRACE("{}", ss.str());
                } break;
                case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: {
                    LOG_INFO("{}", ss.str());
                } break;
                case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: {
                    LOG_WARN("{}", ss.str());
                } break;
                case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: {
                    LOG_ERROR("{}", ss.str());
                } break;
                default:
                    LOG_DEBUG("{}", ss.str());
            }

            return VK_FALSE;
        }

    }

    Instance::Instance(const std::shared_ptr<Window>& window)
        : m_Window(window)
    {
        VK_CHECK(volkInitialize());

        u32 version = volkGetInstanceVersion();

        LOG_INFO("Vulkan instance version: {}.{}.{}",
            VK_VERSION_MAJOR(version),
            VK_VERSION_MINOR(version),
            VK_VERSION_PATCH(version)
        );

        VkApplicationInfo appInfo {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pNext = nullptr,
            .pApplicationName = "RayTracing",
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName = "No Engine",
            .engineVersion = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion = version
        };

        VkDebugUtilsMessengerCreateInfoEXT messengerInfo {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .pNext = nullptr,
            .flags = 0,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = DebugMessengerCallback,
            .pUserData = nullptr
        };

        std::vector<const char*> layers;
        std::vector<const char*> extensions = Window::GetRequiredVulkanExtensions();

#ifndef NDEBUG
        layers.push_back("VK_LAYER_KHRONOS_validation");
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

        VkInstanceCreateInfo instanceInfo {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .pApplicationInfo = &appInfo,
            .enabledLayerCount = static_cast<u32>(layers.size()),
            .ppEnabledLayerNames = layers.data(),
            .enabledExtensionCount = static_cast<u32>(extensions.size()),
            .ppEnabledExtensionNames = extensions.data()
        };

#ifndef NDEBUG
        instanceInfo.pNext = &messengerInfo;
#endif

        VK_CHECK(vkCreateInstance(&instanceInfo, nullptr, &m_Instance));
        volkLoadInstance(m_Instance);

#ifndef NDEBUG
        VK_CHECK(vkCreateDebugUtilsMessengerEXT(m_Instance, &messengerInfo, nullptr, &m_DebugMessenger));
#endif

        VK_CHECK(glfwCreateWindowSurface(m_Instance, m_Window->GetNative(), nullptr, &m_Surface));
    }

    Instance::~Instance()
    {
        vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
#ifndef NDEBUG
        vkDestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);
#endif
        vkDestroyInstance(m_Instance, nullptr);
    }

}
