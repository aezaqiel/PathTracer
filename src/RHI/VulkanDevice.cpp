#include "VulkanDevice.hpp"

#include "VulkanContext.hpp"

namespace PathTracer {

    VulkanDevice::VulkanDevice(std::shared_ptr<VulkanContext> ctx)
        : m_Context(ctx)
    {
        m_RtProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
        m_RtProps.pNext = nullptr;

        u32 deviceCount = 0;
        vkEnumeratePhysicalDevices(m_Context->GetInstance(), &deviceCount, nullptr);
        std::vector<VkPhysicalDevice> availableDevices(deviceCount);
        vkEnumeratePhysicalDevices(m_Context->GetInstance(), &deviceCount, availableDevices.data());

        for (const auto& device : availableDevices) {
            VkPhysicalDeviceProperties2 props;
            props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            props.pNext = &m_RtProps;

            vkGetPhysicalDeviceProperties2(device, &props);

            if (props.properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
                continue;

            u32 queueCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueCount, nullptr);
            std::vector<VkQueueFamilyProperties> availableQueues(queueCount);
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueCount, availableQueues.data());

            std::optional<u32> index;
            for (u32 i = 0; i < queueCount; ++i) {
                const auto& queue = availableQueues.at(i);

                if (!(queue.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                    if (queue.queueFlags & VK_QUEUE_COMPUTE_BIT) {
                        index = i;
                        break;
                    }
                }
            }

            if (index.has_value()) {
                m_PhysicalDevice = device;
                m_QueueFamilyIndex = index.value();

                std::println("Using {}", props.properties.deviceName);
                break;
            }
        }

        f32 queuePriority[] = { 1.0f };

        VkDeviceQueueCreateInfo queueInfo {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueFamilyIndex = m_QueueFamilyIndex,
            .queueCount = 1,
            .pQueuePriorities = queuePriority
        };

        std::vector<const char*> extensions {
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME
        };

        VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddress {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
            .pNext = nullptr,
            .bufferDeviceAddress = VK_TRUE,
            .bufferDeviceAddressCaptureReplay = VK_FALSE,
            .bufferDeviceAddressMultiDevice = VK_FALSE
        };

        VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructure {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
            .pNext = &bufferDeviceAddress,
            .accelerationStructure = VK_TRUE,
            .accelerationStructureCaptureReplay = VK_FALSE,
            .accelerationStructureIndirectBuild = VK_FALSE,
            .accelerationStructureHostCommands = VK_FALSE,
            .descriptorBindingAccelerationStructureUpdateAfterBind = VK_FALSE
        };

        VkPhysicalDeviceRayTracingPipelineFeaturesKHR raytracing {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
            .pNext = &accelerationStructure,
            .rayTracingPipeline = VK_TRUE,
            .rayTracingPipelineShaderGroupHandleCaptureReplay = VK_FALSE,
            .rayTracingPipelineShaderGroupHandleCaptureReplayMixed = VK_FALSE,
            .rayTracingPipelineTraceRaysIndirect = VK_FALSE,
            .rayTraversalPrimitiveCulling = VK_FALSE
        };

        VkPhysicalDeviceSynchronization2Features sync2 {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
            .pNext = &raytracing,
            .synchronization2 = VK_TRUE
        };

        VkPhysicalDeviceDynamicRenderingFeatures dynamicRendering {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
            .pNext = &sync2,
            .dynamicRendering = VK_TRUE,
        };

        VkPhysicalDeviceFeatures2 features {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .pNext = &dynamicRendering,
            .features = {}
        };

        VkDeviceCreateInfo createInfo {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = &features,
            .flags = 0,
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &queueInfo,
            .enabledLayerCount = 0,
            .ppEnabledLayerNames = nullptr,
            .enabledExtensionCount = static_cast<u32>(extensions.size()),
            .ppEnabledExtensionNames = extensions.data(),
            .pEnabledFeatures = nullptr
        };

        VK_CHECK(vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device));
        volkLoadDevice(m_Device);

        vkGetDeviceQueue(m_Device, m_QueueFamilyIndex, 0, &m_QueueFamily);
    }

    VulkanDevice::~VulkanDevice()
    {
        vkDestroyDevice(m_Device, nullptr);
    }

}
