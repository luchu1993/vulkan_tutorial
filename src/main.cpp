#include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>

constexpr  uint32_t WIDTH = 800;
constexpr  uint32_t HEIGHT = 600;

const std::vector<char const*> validationLayers = {
    "VK_LAYER_KHRONOS_validation",
};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

class HelloTriangleApplication {
public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanUp();
    }

private:
    void initWindow() {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    }

    void initVulkan() {
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
        }
    }

    void cleanUp() {
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    void createInstance() {
        constexpr vk::ApplicationInfo appInfo {
            .pApplicationName   = "Hello Triangle",
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName        = "No Engine",
            .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion         =  VK_API_VERSION_1_3
        };

        auto requiredExtensions = getRequiredInstanceExtensions();
        auto extensionProperties = context.enumerateInstanceExtensionProperties();

        for (const auto& extension : requiredExtensions)
        {
            if (std::ranges::none_of(extensionProperties, [&extension](auto const& extensionProperty) {
                    return strcmp(extensionProperty.extensionName, extension) == 0;
                })) 
            {
                throw std::runtime_error("Required extension not supported: " + std::string(extension));
            }
        }
        
        std::vector<char const*> requiredLayers;
        if (enableValidationLayers)
        {
            requiredLayers.assign(validationLayers.begin(), validationLayers.end());
        }
        auto layerProperties = context.enumerateInstanceLayerProperties();
        
        auto unsupportedLayerIt = std::ranges::find_if(requiredLayers, [&layerProperties](auto const& requiredLayer)
        {
            return std::ranges::none_of(layerProperties, [&requiredLayer](auto const& layerProperty)
            {
                return strcmp(layerProperty.layerName, requiredLayer) == 0;
            });
        });
        
        if (unsupportedLayerIt != requiredLayers.end())
        {
            throw std::runtime_error("Required layer not supported: " + std::string(*unsupportedLayerIt));
        }
        
        vk::InstanceCreateInfo createInfo {
            .pApplicationInfo = &appInfo,
            .enabledLayerCount = (uint32_t) requiredLayers.size(),
            .ppEnabledLayerNames = requiredLayers.data(),
            .enabledExtensionCount = (uint32_t) requiredExtensions.size(),
            .ppEnabledExtensionNames = requiredExtensions.data(),
        };

        instance = vk::raii::Instance(context, createInfo);
    }
    
    std::vector<char const*> getRequiredInstanceExtensions()
    {
        uint32_t glfwExtensionCount = 0;
        auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        
        std::vector<char const*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
        if (enableValidationLayers)
        {
            extensions.push_back(vk::EXTDebugUtilsExtensionName);
        }
        return extensions;
    }
    
    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
                                                          vk::DebugUtilsMessageTypeFlagsEXT type,
                                                          const vk::DebugUtilsMessengerCallbackDataEXT* data, 
                                                          void* userData)
    {
        std::cerr << "validation layer: type " << to_string(type) << " msg: " << data->pMessage << "\n";
        return vk::False;
    }
    
    void setupDebugMessenger()
    {
        if (!enableValidationLayers) return;
        vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                                            vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
        vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                                                           vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                                                           vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
        vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT{
            .messageSeverity = severityFlags,
            .messageType     = messageTypeFlags,
            .pfnUserCallback = &debugCallback
        };
        debugMessenger = instance.createDebugUtilsMessengerEXT( debugUtilsMessengerCreateInfoEXT );
    }

    void createSurface() {
        VkSurfaceKHR _surface;
        if (glfwCreateWindowSurface(*instance, window, nullptr, &_surface) != 0) {
            throw std::runtime_error("failed to create window surface");
        }
        surface = vk::raii::SurfaceKHR(instance, _surface);
    }
    
    std::vector<const char*> requiredDeviceExtension = {vk::KHRSwapchainExtensionName};

    bool isDeviceSuitable(vk::raii::PhysicalDevice const& physicalDevice)
    {
        bool supportsVulkan1_3 = physicalDevice.getProperties().apiVersion >= vk::ApiVersion13;
        auto queueFamilies = physicalDevice.getQueueFamilyProperties();
        bool supportsGraphics = std::ranges::any_of( queueFamilies, [](auto const& qfp)
          { return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics); } );

        auto availableDeviceExtensions = physicalDevice.enumerateDeviceExtensionProperties();
        bool supportsAllRequiredExtensions = std::ranges::all_of(requiredDeviceExtension,
            [&availableDeviceExtensions]( auto const & requiredDeviceExtension )
            {
                return std::ranges::any_of(availableDeviceExtensions, [requiredDeviceExtension](auto const& availableDeviceExtension)
                    { return strcmp(availableDeviceExtension.extensionName, requiredDeviceExtension) == 0; });
        });

        auto features = physicalDevice.getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
        bool supportsRequiredFeatures =
              features.get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
              features.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

        return supportsVulkan1_3 && supportsGraphics && supportsAllRequiredExtensions && supportsRequiredFeatures;
    }

    void pickPhysicalDevice()
    {
        std::vector<vk::raii::PhysicalDevice> physicalDevices = instance.enumeratePhysicalDevices();
        auto const devIter = std::ranges::find_if(physicalDevices, [&](auto const& physicalDevice){ return isDeviceSuitable( physicalDevice ); });
        if (devIter == physicalDevices.end() )
        {
            throw std::runtime_error( "failed to find a suitable GPU!" );
        }
        physicalDevice = *devIter;
    }
    
    void createLogicalDevice()
    {
        std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
        uint32_t queueIndex = ~0u;
        for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); ++qfpIndex) {
            if (queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics &&
                physicalDevice.getSurfaceSupportKHR(qfpIndex, *surface)) {
                queueIndex = qfpIndex;
                break;
            }
        }

        if (queueIndex == ~0u) {
            throw std::runtime_error( "Could not find a queue for graphics and present -> terminating");
        }

        float queuePriority = 0.5f;
        vk::DeviceQueueCreateInfo deviceQueueCreateInfo { .queueFamilyIndex = queueIndex, .queueCount = 1, .pQueuePriorities = &queuePriority};
        vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT> featureChain = {
            {},                               
            {.dynamicRendering = true },      
            {.extendedDynamicState = true }   
        };

        vk::DeviceCreateInfo deviceCreateInfo{
            .pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &deviceQueueCreateInfo,
            .enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtension.size()),
            .ppEnabledExtensionNames = requiredDeviceExtension.data()
        };
        
        device = vk::raii::Device(physicalDevice, deviceCreateInfo);
        graphicsQueue = vk::raii::Queue(device, queueIndex, 0);
    }
    
    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(std::vector<vk::SurfaceFormatKHR> const& availableFormats)
    {
        assert(!availableFormats.empty());
        const auto formatIt = std::ranges::find_if(availableFormats, [](const auto& format)
        {
            return format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
        });
        
        return formatIt != availableFormats.end() ? *formatIt : availableFormats[0];
    }
    
    vk::PresentModeKHR chooseSwapPresetMode(std::vector<vk::PresentModeKHR> const& availablePresets)
    {
        assert(std::ranges::any_of(availablePresets, [](auto presetMode) { return presetMode == vk::PresentModeKHR::eFifo; }));
        return std::ranges::any_of(availablePresets, [](auto value) { return value == vk::PresentModeKHR::eMailbox; }) ?
            vk::PresentModeKHR::eMailbox : vk::PresentModeKHR::eFifo;
    }
    
    vk::Extent2D chooseSwapExtent(vk::SurfaceCapabilitiesKHR const& capabilities)
    {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            return capabilities.currentExtent;
        }
        
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        
        return {
            .width = std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
            .height = std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
        };
    }
    
    uint32_t chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const& capabilities)
    {
        auto minImageCount = std::max(3u, capabilities.minImageCount);
        if (capabilities.maxImageCount > 0 && minImageCount > capabilities.maxImageCount)
        {
            minImageCount = capabilities.maxImageCount;
        }
        return minImageCount;
    }
    
    void createSwapChain()
    {
        vk::SurfaceCapabilitiesKHR surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(*surface);
        swapChainExtent = chooseSwapExtent(surfaceCapabilities);
        uint32_t minImageCount = chooseSwapMinImageCount(surfaceCapabilities);
        
        std::vector<vk::SurfaceFormatKHR> availableFormats = physicalDevice.getSurfaceFormatsKHR(*surface);
        swapChainSurfaceFormat = chooseSwapSurfaceFormat(availableFormats);
        
        std::vector<vk::PresentModeKHR> availablePresetModes = physicalDevice.getSurfacePresentModesKHR(*surface);
        
        vk::SwapchainCreateInfoKHR swapChainCreateInfo = {
            .surface = *surface,
            .minImageCount = minImageCount,
            .imageFormat = swapChainSurfaceFormat.format,
            .imageColorSpace = swapChainSurfaceFormat.colorSpace,
            .imageExtent = swapChainExtent,
            .imageArrayLayers = 1,
            .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
            .imageSharingMode = vk::SharingMode::eExclusive,
            .preTransform = surfaceCapabilities.currentTransform,
            .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
            .presentMode = chooseSwapPresetMode(availablePresetModes),
            .clipped = true
        };
        
        swapChain = vk::raii::SwapchainKHR(device, swapChainCreateInfo);
        swapChainImages = swapChain.getImages();
    }
    
    void createImageViews()
    {
        assert(swapChainImageViews.empty());
        vk::ImageViewCreateInfo imageViewCreateInfo = {
            .viewType = vk::ImageViewType::e2D,
            .format = swapChainSurfaceFormat.format,
            .subresourceRange = {
                .aspectMask = vk::ImageAspectFlagBits::eColor, 
                .baseMipLevel = 0, 
                .levelCount = 1,
                .baseArrayLayer = 0, 
                .layerCount = 1
            },
            .components = {
                .r = vk::ComponentSwizzle::eIdentity,
                .g = vk::ComponentSwizzle::eIdentity,
                .b = vk::ComponentSwizzle::eIdentity,
                .a = vk::ComponentSwizzle::eIdentity,
            }
        };
        
        for (auto& image : swapChainImages)
        {
            imageViewCreateInfo.image = image;
            swapChainImageViews.emplace_back(device, imageViewCreateInfo);
        }
    }

private:
    GLFWwindow*                         window                  = nullptr;
    vk::raii::Context                   context;
    vk::raii::Instance                  instance                = nullptr;
    vk::raii::DebugUtilsMessengerEXT    debugMessenger          = nullptr;
    vk::raii::PhysicalDevice            physicalDevice          = nullptr;
    vk::raii::Device                    device                  = nullptr;
    vk::raii::Queue                     graphicsQueue           = nullptr;
    vk::raii::SurfaceKHR                surface                 = nullptr;
    vk::raii::SwapchainKHR              swapChain               = nullptr;
    std::vector<vk::Image>              swapChainImages;
    vk::SurfaceFormatKHR                swapChainSurfaceFormat;
    vk::Extent2D                        swapChainExtent;
    std::vector<vk::raii::ImageView>    swapChainImageViews;
};

int main() {
    try {
        HelloTriangleApplication app;
        app.run();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
