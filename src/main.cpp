#include "ta_timer.hpp"
#include "ta_log.hpp"
#include "vulkan/vulkan.h"
#define SDL_MAIN_HANDLED
#include "SDL/SDL.h"
#include "SDL/SDL_vulkan.h"
#include <algorithm>
#include <cassert>

#define QUERY_AVAILABLE_EXTENSIONS_AND_LAYERS

#define UNUSED(x) (void)(x)

int main(int argc, char *argv[])
{
    UNUSED(argc);
    UNUSED(argv);
    const uint32_t window_w = 1280;
    const uint32_t window_h = 720;

    ta_log_init_file(tg_debug_log, "log.txt", true, true, SRC_ALL, SRC_NONE);

    // Create an SDL window that supports Vulkan rendering.
    ta_log_write(tg_debug_log, SRC_SDL, "Initializing SDL\n");
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        ta_log_write(tg_debug_log, SRC_SDL, "Could not initialize SDL.\n");
        return 1;
    }

    ta_timer_init();

    ta_log_write(tg_debug_log, SRC_SDL, "Creating window\n");
    SDL_Window* window = SDL_CreateWindow("Vulkan Window", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, window_w,
        window_h, SDL_WINDOW_VULKAN);
    if (window == NULL) {
        ta_log_write(tg_debug_log, SRC_SDL, "Could not create SDL window.\n");
        return 1;
    }

    VkResult err = {};

#ifdef QUERY_AVAILABLE_EXTENSIONS_AND_LAYERS
    {
        // Query available instance extensions
        ta_log_write(tg_debug_log, SRC_VULKAN, "Querying available instance extensions\n");
        std::vector<VkExtensionProperties> available_extensions;
        {
            uint32_t available_extensions_count = 0;
            err = vkEnumerateInstanceExtensionProperties(NULL, &available_extensions_count, NULL);
            if (err) {
                ta_log_write(tg_debug_log, SRC_VULKAN, "[%u] Failed to query count of available instance extensions.\n", err);
                return 1;
            }

            available_extensions.resize(available_extensions_count);
            err = vkEnumerateInstanceExtensionProperties(NULL, &available_extensions_count, available_extensions.data());
            if (err) {
                ta_log_write(tg_debug_log, SRC_VULKAN, "[%u] Failed to enumerate available instance extensions.\n", err);
                return 1;
            }
        }
        ta_log_write(tg_debug_log, SRC_VULKAN, "Found %d available extensions:\n", available_extensions.size());
        for (VkExtensionProperties &property : available_extensions) {
            ta_log_write(tg_debug_log, SRC_VULKAN, "    %s\n", property.extensionName);
        }
    }

    {
        // Query available instance layers
        ta_log_write(tg_debug_log, SRC_VULKAN, "Querying available instance layers\n");
        std::vector <VkLayerProperties> available_layers;
        {
            uint32_t available_layers_count = 0;
            err = vkEnumerateInstanceLayerProperties(&available_layers_count, NULL);
            if (err) {
                ta_log_write(tg_debug_log, SRC_VULKAN, "[%u] Failed to query count of available instance layers.\n", err);
                return 1;
            }
            available_layers.resize(available_layers_count);
            err = vkEnumerateInstanceLayerProperties(&available_layers_count, available_layers.data());
            if (err) {
                ta_log_write(tg_debug_log, SRC_VULKAN, "[%u] Failed to enumerate available instance layers.\n", err);
                return 1;
            }
        }
        ta_log_write(tg_debug_log, SRC_VULKAN, "Found %d available layers:\n", available_layers.size());
        for (VkLayerProperties &layer : available_layers) {
            ta_log_write(tg_debug_log, SRC_VULKAN, "    %s\n", layer.layerName);
        }
    }
#endif

    // Get WSI extensions from SDL (we can add more if we like - we just can't remove these)
    // "VK_KHR_surface"
    // "VK_KHR_win32_surface"
    ta_log_write(tg_debug_log, SRC_SDL, "Querying required instance extensions\n");
    std::vector<const char *> extensions;
    {
        uint32_t sdl_extensions_count = 0;
        if (!SDL_Vulkan_GetInstanceExtensions(window, &sdl_extensions_count, NULL)) {
            ta_log_write(tg_debug_log, SRC_SDL, "Failed to query count of required instance extensions for Vulkan.\n");
            return 1;
        }
        extensions.resize(sdl_extensions_count);
        if (!SDL_Vulkan_GetInstanceExtensions(window, &sdl_extensions_count, extensions.data())) {
            ta_log_write(tg_debug_log, SRC_SDL, "Failed to query names of required instance extensions for Vulkan.\n");
            return 1;
        }
    }

    // Use validation layers if this is a debug build
    // HKEY_LOCAL_MACHINE\SOFTWARE\Khronos\Vulkan\ExplicitLayers
    // HKEY_LOCAL_MACHINE\SOFTWARE\Khronos\Vulkan\ImplicitLayers
    std::vector<const char *> layers;
#if _DEBUG
    layers.push_back("VK_LAYER_KHRONOS_validation");
#endif

    // VkApplicationInfo allows the programmer to specifiy some basic information about the
    // program, which can be useful for layers and tools to provide more debug information.
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pNext = NULL;
    appInfo.pApplicationName = "VulkanSandbox";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);;
    appInfo.pEngineName = "RicoTech";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);;
    appInfo.apiVersion = VK_API_VERSION_1_0;

    // VkInstanceCreateInfo is where the programmer specifies the layers and/or extensions that
    // are needed.
    VkInstanceCreateInfo instInfo = {};
    instInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instInfo.pNext = NULL;
    instInfo.flags = 0;
    instInfo.pApplicationInfo = &appInfo;
    instInfo.enabledExtensionCount = (uint32_t)extensions.size();
    instInfo.ppEnabledExtensionNames = extensions.data();
    instInfo.enabledLayerCount = (uint32_t)layers.size();
    instInfo.ppEnabledLayerNames = layers.data();

    // Create the Vulkan instance
    ta_log_write(tg_debug_log, SRC_VULKAN, "Creating Vulkan instance\n");
    VkInstance instance = VK_NULL_HANDLE;
    err = vkCreateInstance(&instInfo, NULL, &instance);
    if (err == VK_ERROR_INCOMPATIBLE_DRIVER) {
        ta_log_write(tg_debug_log, SRC_VULKAN, "[%u] Unable to find a compatible Vulkan driver.\n", err);
        return 1;
    } else if (err) {
        ta_log_write(tg_debug_log, SRC_VULKAN, "[%u] Failed to create Vulkan instance.\n", err);
        return 1;
    }

    // Create a Vulkan surface for rendering
    ta_log_write(tg_debug_log, SRC_SDL, "Creating Vulkan surface\n");
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window, instance, &surface)) {
        ta_log_write(tg_debug_log, SRC_SDL, "Failed to create a surface for Vulkan.\n");
        return 1;
    }

#define VK_VERSION_ARGS(ver) VK_VERSION_MAJOR(ver), VK_VERSION_MINOR(ver), VK_VERSION_PATCH(ver)

    ta_log_write(tg_debug_log, SRC_VULKAN, "Querying avilable physical devices\n");
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    std::vector<VkPhysicalDevice> physical_devices;
    {
        uint32_t physical_devices_count = 0;
        err = vkEnumeratePhysicalDevices(instance, &physical_devices_count, NULL);
        if (err) {
            ta_log_write(tg_debug_log, SRC_VULKAN, "[%u] Failed to query count of available physical devices.\n", err);
            return 1;
        }
        physical_devices.resize(physical_devices_count);
        err = vkEnumeratePhysicalDevices(instance, &physical_devices_count, physical_devices.data());
        if (err) {
            ta_log_write(tg_debug_log, SRC_VULKAN, "[%u] Failed to enumerate available physical devices.\n", err);
            return 1;
        }
    }
    ta_log_write(tg_debug_log, SRC_VULKAN, "Found %d available physical devices:\n", physical_devices.size());

    int queue_family_index = -1;
    for (VkPhysicalDevice &device : physical_devices) {
        VkPhysicalDeviceProperties device_properties = {};
        vkGetPhysicalDeviceProperties(device, &device_properties);
        const char *device_type = "<unknown>";
        switch (device_properties.deviceType) {
            case VK_PHYSICAL_DEVICE_TYPE_OTHER:
                device_type = "OTHER";
                break;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                device_type = "INTEGRATED_GPU";
                break;
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                device_type = "DISCRETE_GPU";
                break;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
                device_type = "VIRTUAL_GPU";
                break;
            case VK_PHYSICAL_DEVICE_TYPE_CPU :
                device_type = "CPU";
                break;
        }
        ta_log_write(tg_debug_log, SRC_VULKAN, "    Device Properties:\n");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        apiVersion      : %u.%u.%u\n", VK_VERSION_ARGS(device_properties.apiVersion));
        ta_log_write(tg_debug_log, SRC_VULKAN, "        driverVersion   : %u.%u.%u\n", VK_VERSION_ARGS(device_properties.driverVersion));
        ta_log_write(tg_debug_log, SRC_VULKAN, "        vendorID        : %u\n",       device_properties.vendorID);
        ta_log_write(tg_debug_log, SRC_VULKAN, "        deviceID        : %u\n",       device_properties.deviceID);
        ta_log_write(tg_debug_log, SRC_VULKAN, "        deviceName      : %s\n",       device_properties.deviceName);
        ta_log_write(tg_debug_log, SRC_VULKAN, "        deviceType      : %d (%s)\n",  device_properties.deviceType, device_type);
        ta_log_write(tg_debug_log, SRC_VULKAN, "        limits:\n");
        ta_log_write(tg_debug_log, SRC_VULKAN, "            maxImageDimension1D                      : %u\n", device_properties.limits.maxImageDimension1D);
        ta_log_write(tg_debug_log, SRC_VULKAN, "            maxImageDimension2D                      : %u\n", device_properties.limits.maxImageDimension2D);
        ta_log_write(tg_debug_log, SRC_VULKAN, "            maxImageDimension3D                      : %u\n", device_properties.limits.maxImageDimension3D);
        ta_log_write(tg_debug_log, SRC_VULKAN, "            TODO: Show the rest of the fields in device_properties.limits\n");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        sparseProperties:\n");
        ta_log_write(tg_debug_log, SRC_VULKAN, "            residencyStandard2DBlockShape            : %s\n", device_properties.sparseProperties.residencyStandard2DBlockShape            ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "            residencyStandard2DMultisampleBlockShape : %s\n", device_properties.sparseProperties.residencyStandard2DMultisampleBlockShape ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "            residencyStandard3DBlockShape            : %s\n", device_properties.sparseProperties.residencyStandard3DBlockShape            ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "            residencyAlignedMipSize                  : %s\n", device_properties.sparseProperties.residencyAlignedMipSize                  ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "            residencyNonResidentStrict               : %s\n", device_properties.sparseProperties.residencyNonResidentStrict               ? "True" : "False");

        VkPhysicalDeviceFeatures device_features = {};
        vkGetPhysicalDeviceFeatures(device, &device_features);
        ta_log_write(tg_debug_log, SRC_VULKAN, "    Device Features:\n");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        robustBufferAccess                      : %s\n", device_features.robustBufferAccess                      ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        fullDrawIndexUint32                     : %s\n", device_features.fullDrawIndexUint32                     ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        imageCubeArray                          : %s\n", device_features.imageCubeArray                          ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        independentBlend                        : %s\n", device_features.independentBlend                        ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        geometryShader                          : %s\n", device_features.geometryShader                          ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        tessellationShader                      : %s\n", device_features.tessellationShader                      ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        sampleRateShading                       : %s\n", device_features.sampleRateShading                       ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        dualSrcBlend                            : %s\n", device_features.dualSrcBlend                            ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        logicOp                                 : %s\n", device_features.logicOp                                 ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        multiDrawIndirect                       : %s\n", device_features.multiDrawIndirect                       ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        drawIndirectFirstInstance               : %s\n", device_features.drawIndirectFirstInstance               ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        depthBiasClamp                          : %s\n", device_features.depthBiasClamp                          ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        depthBiasClamp                          : %s\n", device_features.depthBiasClamp                          ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        fillModeNonSolid                        : %s\n", device_features.fillModeNonSolid                        ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        depthBounds                             : %s\n", device_features.depthBounds                             ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        wideLines                               : %s\n", device_features.wideLines                               ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        largePoints                             : %s\n", device_features.largePoints                             ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        alphaToOne                              : %s\n", device_features.alphaToOne                              ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        multiViewport                           : %s\n", device_features.multiViewport                           ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        samplerAnisotropy                       : %s\n", device_features.samplerAnisotropy                       ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        textureCompressionETC2                  : %s\n", device_features.textureCompressionETC2                  ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        textureCompressionASTC_LDR              : %s\n", device_features.textureCompressionASTC_LDR              ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        textureCompressionBC                    : %s\n", device_features.textureCompressionBC                    ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        occlusionQueryPrecise                   : %s\n", device_features.occlusionQueryPrecise                   ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        pipelineStatisticsQuery                 : %s\n", device_features.pipelineStatisticsQuery                 ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        vertexPipelineStoresAndAtomics          : %s\n", device_features.vertexPipelineStoresAndAtomics          ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        fragmentStoresAndAtomics                : %s\n", device_features.fragmentStoresAndAtomics                ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        shaderTessellationAndGeometryPointSize  : %s\n", device_features.shaderTessellationAndGeometryPointSize  ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        shaderImageGatherExtended               : %s\n", device_features.shaderImageGatherExtended               ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        shaderStorageImageExtendedFormats       : %s\n", device_features.shaderStorageImageExtendedFormats       ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        shaderStorageImageMultisample           : %s\n", device_features.shaderStorageImageMultisample           ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        shaderStorageImageReadWithoutFormat     : %s\n", device_features.shaderStorageImageReadWithoutFormat     ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        shaderStorageImageWriteWithoutFormat    : %s\n", device_features.shaderStorageImageWriteWithoutFormat    ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        shaderUniformBufferArrayDynamicIndexing : %s\n", device_features.shaderUniformBufferArrayDynamicIndexing ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        shaderSampledImageArrayDynamicIndexing  : %s\n", device_features.shaderSampledImageArrayDynamicIndexing  ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        shaderStorageBufferArrayDynamicIndexing : %s\n", device_features.shaderStorageBufferArrayDynamicIndexing ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        shaderStorageImageArrayDynamicIndexing  : %s\n", device_features.shaderStorageImageArrayDynamicIndexing  ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        shaderClipDistance                      : %s\n", device_features.shaderClipDistance                      ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        shaderCullDistance                      : %s\n", device_features.shaderCullDistance                      ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        shaderFloat64                           : %s\n", device_features.shaderFloat64                           ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        shaderInt64                             : %s\n", device_features.shaderInt64                             ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        shaderInt16                             : %s\n", device_features.shaderInt16                             ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        shaderResourceResidency                 : %s\n", device_features.shaderResourceResidency                 ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        shaderResourceMinLod                    : %s\n", device_features.shaderResourceMinLod                    ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        sparseBinding                           : %s\n", device_features.sparseBinding                           ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        sparseResidencyBuffer                   : %s\n", device_features.sparseResidencyBuffer                   ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        sparseResidencyImage2D                  : %s\n", device_features.sparseResidencyImage2D                  ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        sparseResidencyImage3D                  : %s\n", device_features.sparseResidencyImage3D                  ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        sparseResidency2Samples                 : %s\n", device_features.sparseResidency2Samples                 ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        sparseResidency4Samples                 : %s\n", device_features.sparseResidency4Samples                 ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        sparseResidency8Samples                 : %s\n", device_features.sparseResidency8Samples                 ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        sparseResidency16Samples                : %s\n", device_features.sparseResidency16Samples                ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        sparseResidencyAliased                  : %s\n", device_features.sparseResidencyAliased                  ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        variableMultisampleRate                 : %s\n", device_features.variableMultisampleRate                 ? "True" : "False");
        ta_log_write(tg_debug_log, SRC_VULKAN, "        inheritedQueries                        : %s\n", device_features.inheritedQueries                        ? "True" : "False");

        {
            // Query available instance extensions
            ta_log_write(tg_debug_log, SRC_VULKAN, "Querying available instance extensions\n");
            std::vector <VkExtensionProperties> available_device_extensions;
            {
                uint32_t available_device_extensions_count = 0;
                err = vkEnumerateDeviceExtensionProperties(device, NULL, &available_device_extensions_count, NULL);
                if (err) {
                    ta_log_write(tg_debug_log, SRC_VULKAN, "[%u] Failed to query count of available instance extensions.\n", err);
                    return 1;
                }
                available_device_extensions.resize(available_device_extensions_count);
                err = vkEnumerateDeviceExtensionProperties(device, NULL, &available_device_extensions_count, available_device_extensions.data());
                if (err) {
                    ta_log_write(tg_debug_log, SRC_VULKAN, "[%u] Failed to enumerate available instance extensions.\n", err);
                    return 1;
                }
                //DLB_ASSERT(available_device_extensions_count < 32);  // Make sure we passed a big enough array
            }
            ta_log_write(tg_debug_log, SRC_VULKAN, "Found %d available device extensions:\n", available_device_extensions.size());
            for (VkExtensionProperties &extension : available_device_extensions) {
                if (!strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
                    physical_device = device;
                }
                ta_log_write(tg_debug_log, SRC_VULKAN, "    %s\n", extension.extensionName);
            }
        }

        uint32_t queue_family_property_count = 0;
        std::vector <VkQueueFamilyProperties> queue_family_properties;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_property_count, NULL);
        queue_family_properties.resize(queue_family_property_count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_property_count, queue_family_properties.data());

        int i = 0;
        for (VkQueueFamilyProperties &queue_family_property : queue_family_properties) {
            ta_log_write(tg_debug_log, SRC_VULKAN, "    Found %d queues with flags:\n", queue_family_property.queueCount);
            if (queue_family_property.queueFlags & VK_QUEUE_GRAPHICS_BIT      ) ta_log_write(tg_debug_log, SRC_VULKAN, "        %s\n", "GRAPHICS      ");
            if (queue_family_property.queueFlags & VK_QUEUE_COMPUTE_BIT       ) ta_log_write(tg_debug_log, SRC_VULKAN, "        %s\n", "COMPUTE       ");
            if (queue_family_property.queueFlags & VK_QUEUE_TRANSFER_BIT      ) ta_log_write(tg_debug_log, SRC_VULKAN, "        %s\n", "TRANSFER      ");
            if (queue_family_property.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) ta_log_write(tg_debug_log, SRC_VULKAN, "        %s\n", "SPARSE_BINDING");
            if (queue_family_property.queueFlags & VK_QUEUE_PROTECTED_BIT     ) ta_log_write(tg_debug_log, SRC_VULKAN, "        %s\n", "PROTECTED_BIT ");

            if (queue_family_property.queueFlags & VK_QUEUE_GRAPHICS_BIT && queue_family_index == -1) {
                VkBool32 present_supported = false;
                err = vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_supported);
                if (err) {
                    ta_log_write(tg_debug_log, SRC_VULKAN, "[%u] Failed to query physical device surface support.\n", err);
                    return 1;
                }
                if (present_supported) {
                    // NOTE(saidwho12): Doesn't work on NVIDIA DGX-2
                    queue_family_index = i;
                }
            }
        }
    }

    assert(physical_device != VK_NULL_HANDLE);
    assert(queue_family_index >= 0);

    // Create one graphics queue
    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_create_info = {};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = queue_family_index;
    queue_create_info.queueCount = 1;
    queue_create_info.pQueuePriorities = &queue_priority;

    // TODO: Set device feature flags to VK_TRUE for features we want
    VkPhysicalDeviceFeatures device_features = {};

    std::vector<const char *> device_extensions;
    device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    // Create logical device
    VkDeviceCreateInfo device_create_info = {};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.pQueueCreateInfos = &queue_create_info;
    device_create_info.queueCreateInfoCount = 1;
    device_create_info.pEnabledFeatures = &device_features;
    device_create_info.enabledExtensionCount = (uint32_t)device_extensions.size();
    device_create_info.ppEnabledExtensionNames = device_extensions.data();
#if _DEBUG
    device_create_info.enabledLayerCount = (uint32_t)layers.size();
    device_create_info.ppEnabledLayerNames = layers.data();
#else
    createInfo.enabledLayerCount = 0;
#endif

    VkDevice logical_device = VK_NULL_HANDLE;
    err = vkCreateDevice(physical_device, &device_create_info, NULL, &logical_device);
    if (err) {
        ta_log_write(tg_debug_log, SRC_VULKAN, "[%u] Failed to create logical device.\n", err);
        return 1;
    }

    VkQueue queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(logical_device, queue_family_index, 0, &queue);

    struct swap_chain_t {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> present_modes;
        VkExtent2D extent;
        VkSwapchainKHR swap_chain;
    };
    struct swap_chain_t swap_chain = {};

    err = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &swap_chain.capabilities);
    if (err) {
        ta_log_write(tg_debug_log, SRC_VULKAN, "[%u] Failed to query physical device surface capabitilities.\n", err);
        return 1;
    }

    VkSurfaceFormatKHR *surface_format = NULL;
    uint32_t swap_chain_format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &swap_chain_format_count, NULL);
    assert(swap_chain_format_count);
    swap_chain.formats.resize(swap_chain_format_count);
    err = vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &swap_chain_format_count, swap_chain.formats.data());
    if (err) {
        ta_log_write(tg_debug_log, SRC_VULKAN, "[%u] Failed to query physical device surface formats.\n", err);
        return 1;
    }
    for (VkSurfaceFormatKHR &format : swap_chain.formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            surface_format = &format;
        }
    }
    assert(surface_format);

    bool triple_buffer_try = true;
    VkPresentModeKHR surface_present_mode = VK_PRESENT_MODE_FIFO_KHR;
    if (triple_buffer_try) {
        uint32_t swapchain_present_mode_count = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &swapchain_present_mode_count, NULL);
        assert(swapchain_present_mode_count);
        swap_chain.present_modes.resize(swapchain_present_mode_count);
        err = vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &swapchain_present_mode_count, swap_chain.present_modes.data());
        if (err) {
            ta_log_write(tg_debug_log, SRC_VULKAN, "[%u] Failed to query physical device surface present modes.\n", err);
            return 1;
        }
        for (VkPresentModeKHR &mode : swap_chain.present_modes) {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                surface_present_mode = mode;
            }
        }
        assert(VK_PRESENT_MODE_MAILBOX_KHR);
    }

    if (swap_chain.capabilities.currentExtent.width != UINT32_MAX) {
        swap_chain.extent = swap_chain.capabilities.currentExtent;
    } else {
        swap_chain.extent.width = std::max(swap_chain.capabilities.minImageExtent.width, std::min(swap_chain.capabilities.maxImageExtent.width, window_w));
        swap_chain.extent.height = std::max(swap_chain.capabilities.minImageExtent.height, std::min(swap_chain.capabilities.maxImageExtent.height, window_h));
    }

    // https://vulkan-tutorial.com/en/Drawing_a_triangle/Presentation/Swap_chain
    // Choosing the right settings for the swap chain

    uint32_t swap_chain_image_count = 2;
    if (triple_buffer_try) {
        swap_chain_image_count++;
    }
    assert(swap_chain_image_count >= swap_chain.capabilities.minImageCount);
    if (swap_chain.capabilities.maxImageCount) {
        swap_chain_image_count = std::min(swap_chain_image_count, swap_chain.capabilities.maxImageCount);
    }

    VkSwapchainCreateInfoKHR swap_chain_create_info = {};
    swap_chain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swap_chain_create_info.surface = surface;
    swap_chain_create_info.minImageCount = swap_chain_image_count;
    swap_chain_create_info.imageFormat = surface_format->format;
    swap_chain_create_info.imageColorSpace = surface_format->colorSpace;
    swap_chain_create_info.imageExtent = swap_chain.extent;
    swap_chain_create_info.imageArrayLayers = 1;
    swap_chain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    // NOTE: Assumes graphics_queue == present_queue, which we are currently
    // guaranteeing by only having a single index and asserting when not found.
    swap_chain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swap_chain_create_info.preTransform = swap_chain.capabilities.currentTransform;
    swap_chain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swap_chain_create_info.presentMode = surface_present_mode;
    // NOTE: May want to disable surface clipping if we do e.g. screenshots.
    swap_chain_create_info.clipped = VK_TRUE;
    // NOTE: This is used when recreating the swap chain, e.g. on window resize
    swap_chain_create_info.oldSwapchain = VK_NULL_HANDLE;

    err = vkCreateSwapchainKHR(logical_device, &swap_chain_create_info, NULL, &swap_chain.swap_chain);
    if (err) {
        ta_log_write(tg_debug_log, SRC_VULKAN, "[%u] Failed to create swap chain.\n", err);
        return 1;
    }

    ta_log_write(tg_debug_log, SRC_VULKAN, "We got a swapchain bois.\n");

    // Poll for user input
    bool stillRunning = true;
    while(stillRunning) {
        SDL_Event event = {};
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    stillRunning = false;
                    break;
                default:
                    // Do nothing
                    break;
            }
        }
        SDL_Delay(10);
    }

    // Clean up
    vkDestroySwapchainKHR(logical_device, swap_chain.swap_chain, NULL);
    vkDestroySurfaceKHR(instance, surface, NULL);
    SDL_DestroyWindow(window);
    SDL_Quit();
    vkDestroyDevice(logical_device, NULL);
    vkDestroyInstance(instance, NULL);
    return 0;
}
