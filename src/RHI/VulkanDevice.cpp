#include "VulkanDevice.hpp"

#include "VulkanContext.hpp"
#include "Core/Logger.hpp"

namespace PathTracer {

    VulkanDevice::VulkanDevice(std::shared_ptr<VulkanContext> ctx)
        : m_Context(ctx)
    {
        std::vector<const char*> extensions {
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME
        };

        m_RtProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
        m_RtProps.pNext = nullptr;

        u32 deviceCount = 0;
        vkEnumeratePhysicalDevices(m_Context->GetInstance(), &deviceCount, nullptr);
        std::vector<VkPhysicalDevice> availableDevices(deviceCount);
        vkEnumeratePhysicalDevices(m_Context->GetInstance(), &deviceCount, availableDevices.data());

        for (const auto& device : availableDevices) {
            if (!CheckDeviceExtensionSupport(device, extensions)) {
                continue;
            }

            VkPhysicalDeviceProperties2 props;
            props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            props.pNext = &m_RtProps;

            vkGetPhysicalDeviceProperties2(device, &props);

            if (props.properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                continue;
            }

            u32 queueCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueCount, nullptr);
            std::vector<VkQueueFamilyProperties> availableQueues(queueCount);
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueCount, availableQueues.data());

            QueueFamilyIndices indices = FindQueueFamilies(device, m_Context->GetSurface());

            if (indices.IsSufficient()) {
                m_PhysicalDevice = device;
                m_QueueIndices = indices;

                LOG_INFO("Physical device: {}", props.properties.deviceName);
                LOG_INFO("Graphics queue: {}", m_QueueIndices.graphics.value());
                LOG_INFO("Present queue: {}", m_QueueIndices.present.value());
                LOG_INFO("Compute queue: {}", m_QueueIndices.compute.value());
                LOG_INFO("Transfer queue: {}", m_QueueIndices.transfer.value());

                break;
            }
        }

        f32 queuePriority[] = { 1.0f };

        std::vector<VkDeviceQueueCreateInfo> queueInfos;
        queueInfos.reserve(m_QueueIndices.GetUniqueIndices().size());

        for (const auto& index : m_QueueIndices.GetUniqueIndices()) {
            queueInfos.push_back(VkDeviceQueueCreateInfo {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .queueFamilyIndex = index,
                .queueCount = 1,
                .pQueuePriorities = queuePriority
            });
        }

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
            .queueCreateInfoCount = static_cast<u32>(queueInfos.size()),
            .pQueueCreateInfos = queueInfos.data(),
            .enabledLayerCount = 0,
            .ppEnabledLayerNames = nullptr,
            .enabledExtensionCount = static_cast<u32>(extensions.size()),
            .ppEnabledExtensionNames = extensions.data(),
            .pEnabledFeatures = nullptr
        };

        VK_CHECK(vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device));
        volkLoadDevice(m_Device);

        VmaAllocatorCreateInfo allocatorInfo {
            .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
            .physicalDevice = m_PhysicalDevice,
            .device = m_Device,
            .preferredLargeHeapBlockSize = 0,
            .pAllocationCallbacks = nullptr,
            .pDeviceMemoryCallbacks = nullptr,
            .pHeapSizeLimit = nullptr,
            .pVulkanFunctions = nullptr,
            .instance = m_Context->GetInstance(),
            .vulkanApiVersion = volkGetInstanceVersion()
        };

        VmaVulkanFunctions vmaFuncs;
        vmaImportVulkanFunctionsFromVolk(&allocatorInfo, &vmaFuncs);
        allocatorInfo.pVulkanFunctions = &vmaFuncs;

        VK_CHECK(vmaCreateAllocator(&allocatorInfo, &m_Allocator));

        vkGetDeviceQueue(m_Device, m_QueueIndices.graphics.value(), 0, &m_GraphicsQueue);
        vkGetDeviceQueue(m_Device, m_QueueIndices.present.value(), 0, &m_PresentQueue);
        vkGetDeviceQueue(m_Device, m_QueueIndices.compute.value(), 0, &m_ComputeQueue);
        vkGetDeviceQueue(m_Device, m_QueueIndices.transfer.value(), 0, &m_TransferQueue);
    }

    VulkanDevice::~VulkanDevice()
    {
        vmaDestroyAllocator(m_Allocator);
        vkDestroyDevice(m_Device, nullptr);
    }

    bool VulkanDevice::CheckDeviceExtensionSupport(VkPhysicalDevice device, const std::vector<const char*>& extensions)
    {
        u32 extensionCount = 0;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> remainingExtensions(extensions.begin(), extensions.end());

        for (const auto& ext : availableExtensions) {
            remainingExtensions.erase(ext.extensionName);
        }

        return remainingExtensions.empty();
    }

    VulkanDevice::QueueFamilyIndices VulkanDevice::FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
    {
        QueueFamilyIndices indices;

        u32 queueCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueCount, queueFamilies.data());

        std::optional<u32> dedicatedCompute;
        std::optional<u32> dedicatedTransfer;
        std::optional<u32> anyCompute;
        std::optional<u32> anyTransfer;

        u32 index = 0;
        for (const auto& queue : queueFamilies) {
            VkBool32 present = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, index, surface, &present);
            if (present == VK_TRUE) {
                indices.present = index;
            }

            if (queue.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                if (!indices.graphics.has_value()) {
                    indices.graphics = index;
                }
            }

            if (queue.queueFlags & VK_QUEUE_COMPUTE_BIT) {
                if (!(queue.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                    if (!dedicatedCompute.has_value()) {
                        dedicatedCompute = index;
                    }
                } else {
                    if (!anyCompute.has_value()) {
                        anyCompute = index;
                    }
                }
            }

            if (queue.queueFlags & VK_QUEUE_TRANSFER_BIT) {
                if (!(queue.queueFlags & VK_QUEUE_GRAPHICS_BIT) && !(queue.queueFlags & VK_QUEUE_COMPUTE_BIT)) {
                    if (!dedicatedTransfer.has_value()) {
                        dedicatedTransfer = index;
                    }
                } else {
                    if (!anyTransfer.has_value()) {
                        anyTransfer = index;
                    }
                }
            }

            index++;
        }

        if (dedicatedCompute.has_value()) {
            indices.compute = dedicatedCompute;
        } else if (anyCompute.has_value()) {
            indices.compute = anyCompute;
        } else if (indices.graphics.has_value()) {
            indices.compute = indices.graphics;
        }

        if (dedicatedTransfer.has_value()) {
            indices.transfer = dedicatedTransfer;
        } else if (anyTransfer.has_value()) {
            indices.transfer = anyTransfer;
        } else if (indices.compute.has_value() && indices.compute != indices.graphics) {
            indices.transfer = indices.compute;
        } else {
            indices.transfer = indices.graphics;
        }

        return indices;
    }

}
