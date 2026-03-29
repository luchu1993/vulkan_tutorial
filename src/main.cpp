#include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <fstream>

constexpr uint32_t WIDTH  = 800;
constexpr uint32_t HEIGHT = 600;

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

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
    void run()
    {
        initWindow();
        initVulkan();
        mainLoop();
        cleanUp();
    }

private:
    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    void initWindow()
    {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    }

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height)
    {
        auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
        app->framebufferResized = true;
    }

    void initVulkan()
    {
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createGraphicsPipeline();
        createCommandPool();
        createCommandBuffers();
        createSyncObjects();
    }

    void mainLoop()
    {
        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();
            drawFrame();
        }
        device.waitIdle();
    }

    void cleanUp()
    {
        device.waitIdle();
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    // -------------------------------------------------------------------------
    // Render loop
    // -------------------------------------------------------------------------

    void drawFrame()
    {
        // Note: inFlightFences, presentCompleteSemaphores, and commandBuffers are indexed by frameIndex,
        //       while renderFinishedSemaphores is indexed by imageIndex
        auto fenceResult = device.waitForFences(*inFlightFences[frameIndex], vk::True, UINT64_MAX);
        if (fenceResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to wait for fence!");
        }

        auto [result, imageIndex] = swapChain.acquireNextImage(UINT64_MAX, *presentCompleteSemaphores[frameIndex], nullptr);

        // Due to VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS being defined, eErrorOutOfDateKHR can be checked as a result
        // here and does not need to be caught by an exception.
        if (result == vk::Result::eErrorOutOfDateKHR)
        {
            recreateSwapChain();
            return;
        }
        if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
        {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        device.resetFences(*inFlightFences[frameIndex]);

        commandBuffers[frameIndex].reset();
        recordCommandBuffer(imageIndex);

        vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
        const vk::SubmitInfo submitInfo{
            .waitSemaphoreCount   = 1,
            .pWaitSemaphores      = &*presentCompleteSemaphores[frameIndex],
            .pWaitDstStageMask    = &waitDestinationStageMask,
            .commandBufferCount   = 1,
            .pCommandBuffers      = &*commandBuffers[frameIndex],
            .signalSemaphoreCount = 1,
            .pSignalSemaphores    = &*renderFinishedSemaphores[imageIndex],
        };
        queue.submit(submitInfo, *inFlightFences[frameIndex]);

        const vk::PresentInfoKHR presentInfoKHR{
            .waitSemaphoreCount = 1,
            .pWaitSemaphores    = &*renderFinishedSemaphores[imageIndex],
            .swapchainCount     = 1,
            .pSwapchains        = &*swapChain,
            .pImageIndices      = &imageIndex,
            .pResults           = nullptr,
        };
        result = queue.presentKHR(presentInfoKHR);
        if (result == vk::Result::eSuboptimalKHR || result == vk::Result::eErrorOutOfDateKHR || framebufferResized)
        {
            framebufferResized = false;
            recreateSwapChain();
        }

        frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void recordCommandBuffer(uint32_t imageIndex)
    {
        auto& commandBuffer = commandBuffers[frameIndex];
        commandBuffer.begin({});

        transitionImageLayout(
            imageIndex,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eColorAttachmentOptimal,
            {},
            vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::PipelineStageFlagBits2::eColorAttachmentOutput
        );

        vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
        vk::RenderingAttachmentInfo attachmentInfo{
            .imageView   = swapChainImageViews[imageIndex],
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp      = vk::AttachmentLoadOp::eClear,
            .storeOp     = vk::AttachmentStoreOp::eStore,
            .clearValue  = clearColor,
        };
        vk::RenderingInfo renderingInfo{
            .renderArea           = {.offset = {0, 0}, .extent = swapChainExtent},
            .layerCount           = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments    = &attachmentInfo,
        };

        commandBuffer.beginRendering(renderingInfo);
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline);
        commandBuffer.setViewport(0, vk::Viewport{
            .x      = 0.0f,
            .y      = 0.0f,
            .width  = static_cast<float>(swapChainExtent.width),
            .height = static_cast<float>(swapChainExtent.height),
        });
        commandBuffer.setScissor(0, vk::Rect2D{
            .offset = {.x = 0, .y = 0},
            .extent = swapChainExtent,
        });
        commandBuffer.draw(3, 1, 0, 0);
        commandBuffer.endRendering();

        transitionImageLayout(
            imageIndex,
            vk::ImageLayout::eColorAttachmentOptimal,
            vk::ImageLayout::ePresentSrcKHR,
            vk::AccessFlagBits2::eColorAttachmentWrite,
            {},
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::PipelineStageFlagBits2::eBottomOfPipe
        );

        commandBuffer.end();
    }

    void transitionImageLayout(
        uint32_t              imageIndex,
        vk::ImageLayout       oldLayout,
        vk::ImageLayout       newLayout,
        vk::AccessFlags2      srcAccessMask,
        vk::AccessFlags2      dstAccessMask,
        vk::PipelineStageFlags2 srcStageMask,
        vk::PipelineStageFlags2 dstStageMask)
    {
        vk::ImageMemoryBarrier2 barrier{
            .srcStageMask        = srcStageMask,
            .srcAccessMask       = srcAccessMask,
            .dstStageMask        = dstStageMask,
            .dstAccessMask       = dstAccessMask,
            .oldLayout           = oldLayout,
            .newLayout           = newLayout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image               = swapChainImages[imageIndex],
            .subresourceRange    = {
                .aspectMask     = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1,
            },
        };
        vk::DependencyInfo dependencyInfo{
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers    = &barrier,
        };
        commandBuffers[frameIndex].pipelineBarrier2(dependencyInfo);
    }

    // -------------------------------------------------------------------------
    // Instance
    // -------------------------------------------------------------------------

    std::vector<char const*> getRequiredInstanceExtensions()
    {
        uint32_t glfwExtensionCount = 0;
        auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<char const*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
        if (enableValidationLayers)
            extensions.push_back(vk::EXTDebugUtilsExtensionName);
        return extensions;
    }

    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT  severity,
        vk::DebugUtilsMessageTypeFlagsEXT         type,
        const vk::DebugUtilsMessengerCallbackDataEXT* data,
        void* userData)
    {
        std::cerr << "validation layer: type " << to_string(type) << " msg: " << data->pMessage << "\n";
        return vk::False;
    }

    void createInstance()
    {
        constexpr vk::ApplicationInfo appInfo{
            .pApplicationName   = "Hello Triangle",
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName        = "No Engine",
            .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion         = VK_API_VERSION_1_3,
        };

        auto requiredExtensions   = getRequiredInstanceExtensions();
        auto extensionProperties  = context.enumerateInstanceExtensionProperties();
        for (const auto& extension : requiredExtensions)
        {
            if (std::ranges::none_of(extensionProperties, [&extension](auto const& ep) {
                    return strcmp(ep.extensionName, extension) == 0;
                }))
                throw std::runtime_error("Required extension not supported: " + std::string(extension));
        }

        std::vector<char const*> requiredLayers;
        if (enableValidationLayers)
            requiredLayers.assign(validationLayers.begin(), validationLayers.end());

        auto layerProperties   = context.enumerateInstanceLayerProperties();
        auto unsupportedLayerIt = std::ranges::find_if(requiredLayers, [&layerProperties](auto const& requiredLayer) {
            return std::ranges::none_of(layerProperties, [&requiredLayer](auto const& lp) {
                return strcmp(lp.layerName, requiredLayer) == 0;
            });
        });
        if (unsupportedLayerIt != requiredLayers.end())
            throw std::runtime_error("Required layer not supported: " + std::string(*unsupportedLayerIt));

        vk::InstanceCreateInfo createInfo{
            .pApplicationInfo        = &appInfo,
            .enabledLayerCount       = static_cast<uint32_t>(requiredLayers.size()),
            .ppEnabledLayerNames     = requiredLayers.data(),
            .enabledExtensionCount   = static_cast<uint32_t>(requiredExtensions.size()),
            .ppEnabledExtensionNames = requiredExtensions.data(),
        };
        instance = vk::raii::Instance(context, createInfo);
    }

    void setupDebugMessenger()
    {
        if (!enableValidationLayers) return;

        vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
        vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral     |
            vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
            vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);

        vk::DebugUtilsMessengerCreateInfoEXT createInfo{
            .messageSeverity = severityFlags,
            .messageType     = messageTypeFlags,
            .pfnUserCallback = &debugCallback,
        };
        debugMessenger = instance.createDebugUtilsMessengerEXT(createInfo);
    }

    // -------------------------------------------------------------------------
    // Device
    // -------------------------------------------------------------------------

    bool isDeviceSuitable(vk::raii::PhysicalDevice const& device)
    {
        bool supportsVulkan1_3 = device.getProperties().apiVersion >= vk::ApiVersion13;

        auto queueFamilies   = device.getQueueFamilyProperties();
        bool supportsGraphics = std::ranges::any_of(queueFamilies, [](auto const& qfp) {
            return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics);
        });

        auto availableDeviceExtensions    = device.enumerateDeviceExtensionProperties();
        bool supportsAllRequiredExtensions = std::ranges::all_of(requiredDeviceExtensions,
            [&availableDeviceExtensions](auto const& required) {
                return std::ranges::any_of(availableDeviceExtensions, [required](auto const& available) {
                    return strcmp(available.extensionName, required) == 0;
                });
            });

        auto features = device.getFeatures2<
            vk::PhysicalDeviceFeatures2,
            vk::PhysicalDeviceVulkan11Features,
            vk::PhysicalDeviceVulkan13Features,
            vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
        bool supportsRequiredFeatures =
            features.get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
            features.get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering     &&
            features.get<vk::PhysicalDeviceVulkan13Features>().synchronization2     &&
            features.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

        return supportsVulkan1_3 && supportsGraphics && supportsAllRequiredExtensions && supportsRequiredFeatures;
    }

    void createSurface()
    {
        VkSurfaceKHR _surface;
        if (glfwCreateWindowSurface(*instance, window, nullptr, &_surface) != 0)
            throw std::runtime_error("failed to create window surface");
        surface = vk::raii::SurfaceKHR(instance, _surface);
    }

    void pickPhysicalDevice()
    {
        std::vector<vk::raii::PhysicalDevice> physicalDevices = instance.enumeratePhysicalDevices();
        auto const devIter = std::ranges::find_if(physicalDevices, [&](auto const& pd) {
            return isDeviceSuitable(pd);
        });
        if (devIter == physicalDevices.end())
            throw std::runtime_error("failed to find a suitable GPU!");
        physicalDevice = *devIter;
    }

    void createLogicalDevice()
    {
        auto queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
        uint32_t queueIndex = ~0u;
        for (uint32_t i = 0; i < queueFamilyProperties.size(); ++i)
        {
            if (queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics &&
                physicalDevice.getSurfaceSupportKHR(i, *surface))
            {
                queueIndex = i;
                break;
            }
        }
        if (queueIndex == ~0u)
            throw std::runtime_error("Could not find a queue for graphics and present -> terminating");
        graphicsIndex = queueIndex;

        float queuePriority = 0.5f;
        vk::DeviceQueueCreateInfo deviceQueueCreateInfo{
            .queueFamilyIndex = queueIndex,
            .queueCount       = 1,
            .pQueuePriorities = &queuePriority,
        };

        vk::StructureChain<
            vk::PhysicalDeviceFeatures2,
            vk::PhysicalDeviceVulkan11Features,
            vk::PhysicalDeviceVulkan13Features,
            vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT> featureChain = {
            {},
            {.shaderDrawParameters = true},
            {.synchronization2 = true, .dynamicRendering = true},
            {.extendedDynamicState = true},
        };

        vk::DeviceCreateInfo deviceCreateInfo{
            .pNext                   = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
            .queueCreateInfoCount    = 1,
            .pQueueCreateInfos       = &deviceQueueCreateInfo,
            .enabledExtensionCount   = static_cast<uint32_t>(requiredDeviceExtensions.size()),
            .ppEnabledExtensionNames = requiredDeviceExtensions.data(),
        };

        device = vk::raii::Device(physicalDevice, deviceCreateInfo);
        queue  = vk::raii::Queue(device, queueIndex, 0);
    }

    // -------------------------------------------------------------------------
    // Swap chain
    // -------------------------------------------------------------------------

    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(std::vector<vk::SurfaceFormatKHR> const& availableFormats)
    {
        assert(!availableFormats.empty());
        const auto it = std::ranges::find_if(availableFormats, [](const auto& format) {
            return format.format     == vk::Format::eB8G8R8A8Srgb &&
                   format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
        });
        return it != availableFormats.end() ? *it : availableFormats[0];
    }

    vk::PresentModeKHR chooseSwapPresentMode(std::vector<vk::PresentModeKHR> const& availableModes)
    {
        assert(std::ranges::any_of(availableModes, [](auto m) { return m == vk::PresentModeKHR::eFifo; }));
        return std::ranges::any_of(availableModes, [](auto m) { return m == vk::PresentModeKHR::eMailbox; })
            ? vk::PresentModeKHR::eMailbox
            : vk::PresentModeKHR::eFifo;
    }

    vk::Extent2D chooseSwapExtent(vk::SurfaceCapabilitiesKHR const& capabilities)
    {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
            return capabilities.currentExtent;

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        return {
            .width  = std::clamp<uint32_t>(width,  capabilities.minImageExtent.width,  capabilities.maxImageExtent.width),
            .height = std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height),
        };
    }

    uint32_t chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const& capabilities)
    {
        auto minImageCount = std::max(3u, capabilities.minImageCount);
        if (capabilities.maxImageCount > 0 && minImageCount > capabilities.maxImageCount)
            minImageCount = capabilities.maxImageCount;
        return minImageCount;
    }

    void createSwapChain()
    {
        vk::SurfaceCapabilitiesKHR surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(*surface);
        swapChainExtent       = chooseSwapExtent(surfaceCapabilities);
        uint32_t minImageCount = chooseSwapMinImageCount(surfaceCapabilities);

        auto availableFormats      = physicalDevice.getSurfaceFormatsKHR(*surface);
        swapChainSurfaceFormat     = chooseSwapSurfaceFormat(availableFormats);
        auto availablePresentModes = physicalDevice.getSurfacePresentModesKHR(*surface);

        vk::SwapchainCreateInfoKHR createInfo{
            .surface          = *surface,
            .minImageCount    = minImageCount,
            .imageFormat      = swapChainSurfaceFormat.format,
            .imageColorSpace  = swapChainSurfaceFormat.colorSpace,
            .imageExtent      = swapChainExtent,
            .imageArrayLayers = 1,
            .imageUsage       = vk::ImageUsageFlagBits::eColorAttachment,
            .imageSharingMode = vk::SharingMode::eExclusive,
            .preTransform     = surfaceCapabilities.currentTransform,
            .compositeAlpha   = vk::CompositeAlphaFlagBitsKHR::eOpaque,
            .presentMode      = chooseSwapPresentMode(availablePresentModes),
            .clipped          = true,
        };

        swapChain       = vk::raii::SwapchainKHR(device, createInfo);
        swapChainImages = swapChain.getImages();
    }

    void cleanupSwapChain()
    {
        swapChainImageViews.clear();
        swapChain = nullptr;
    }

    void recreateSwapChain()
    {
        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        while (width == 0 || height == 0)
        {
            glfwGetFramebufferSize(window, &width, &height);
            glfwWaitEvents();
        }

        device.waitIdle();

        cleanupSwapChain();
        createSwapChain();
        createImageViews();
    }

    void createImageViews()
    {
        assert(swapChainImageViews.empty());
        vk::ImageViewCreateInfo createInfo{
            .viewType         = vk::ImageViewType::e2D,
            .format           = swapChainSurfaceFormat.format,
            .components       = {vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity,
                                 vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity},
            .subresourceRange = {
                .aspectMask     = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1,
            },
        };
        for (auto& image : swapChainImages)
        {
            createInfo.image = image;
            swapChainImageViews.emplace_back(device, createInfo);
        }
    }

    // -------------------------------------------------------------------------
    // Pipeline
    // -------------------------------------------------------------------------

    static std::vector<char> readFile(const std::string& filename)
    {
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file.is_open())
            throw std::runtime_error("failed to open file");

        std::vector<char> buffer(file.tellg());
        file.seekg(0, std::ios::beg);
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        return buffer;
    }

    vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) const
    {
        vk::ShaderModuleCreateInfo createInfo{
            .codeSize = code.size(),
            .pCode    = reinterpret_cast<const uint32_t*>(code.data()),
        };
        return vk::raii::ShaderModule(device, createInfo);
    }

    void createGraphicsPipeline()
    {
        vk::raii::ShaderModule shaderModule = createShaderModule(readFile("shaders/slang.spv"));

        vk::PipelineShaderStageCreateInfo shaderStages[] = {
            {.stage = vk::ShaderStageFlagBits::eVertex,   .module = shaderModule, .pName = "vertMain"},
            {.stage = vk::ShaderStageFlagBits::eFragment, .module = shaderModule, .pName = "fragMain"},
        };

        vk::PipelineVertexInputStateCreateInfo   vertexInputInfo;
        vk::PipelineInputAssemblyStateCreateInfo inputAssembly{.topology = vk::PrimitiveTopology::eTriangleList};
        vk::PipelineViewportStateCreateInfo      viewportState{.viewportCount = 1, .scissorCount = 1};
        vk::PipelineRasterizationStateCreateInfo rasterizer{
            .depthClampEnable        = vk::False,
            .rasterizerDiscardEnable = vk::False,
            .polygonMode             = vk::PolygonMode::eFill,
            .cullMode                = vk::CullModeFlagBits::eBack,
            .frontFace               = vk::FrontFace::eClockwise,
            .depthBiasEnable         = vk::False,
            .lineWidth               = 1.f,
        };
        vk::PipelineMultisampleStateCreateInfo multisampling{
            .rasterizationSamples = vk::SampleCountFlagBits::e1,
            .sampleShadingEnable  = vk::False,
        };
        vk::PipelineColorBlendAttachmentState colorBlendAttachment{
            .blendEnable    = vk::False,
            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
        };
        vk::PipelineColorBlendStateCreateInfo colorBlending{
            .logicOpEnable   = vk::False,
            .logicOp         = vk::LogicOp::eCopy,
            .attachmentCount = 1,
            .pAttachments    = &colorBlendAttachment,
        };

        std::vector<vk::DynamicState> dynamicStateList{vk::DynamicState::eViewport, vk::DynamicState::eScissor};
        vk::PipelineDynamicStateCreateInfo dynamicState{
            .dynamicStateCount = static_cast<uint32_t>(dynamicStateList.size()),
            .pDynamicStates    = dynamicStateList.data(),
        };

        pipelineLayout = vk::raii::PipelineLayout(device, vk::PipelineLayoutCreateInfo{});

        vk::PipelineRenderingCreateInfo renderingCreateInfo{
            .colorAttachmentCount    = 1,
            .pColorAttachmentFormats = &swapChainSurfaceFormat.format,
        };
        vk::GraphicsPipelineCreateInfo pipelineInfo{
            .pNext               = &renderingCreateInfo,
            .stageCount          = 2,
            .pStages             = shaderStages,
            .pVertexInputState   = &vertexInputInfo,
            .pInputAssemblyState = &inputAssembly,
            .pViewportState      = &viewportState,
            .pRasterizationState = &rasterizer,
            .pMultisampleState   = &multisampling,
            .pColorBlendState    = &colorBlending,
            .pDynamicState       = &dynamicState,
            .layout              = pipelineLayout,
            .renderPass          = nullptr,
        };

        graphicsPipeline = vk::raii::Pipeline(device, nullptr, pipelineInfo);
    }

    // -------------------------------------------------------------------------
    // Commands & sync
    // -------------------------------------------------------------------------

    void createCommandPool()
    {
        vk::CommandPoolCreateInfo poolInfo{
            .flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = graphicsIndex,
        };
        commandPool = vk::raii::CommandPool(device, poolInfo);
    }

    void createCommandBuffers()
    {
        vk::CommandBufferAllocateInfo allocInfo{
            .commandPool        = commandPool,
            .level              = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = MAX_FRAMES_IN_FLIGHT,
        };
        commandBuffers = vk::raii::CommandBuffers(device, allocInfo);
    }

    void createSyncObjects()
    {
        assert(presentCompleteSemaphores.empty() && renderFinishedSemaphores.empty());
        for (size_t i = 0; i < swapChainImages.size(); ++i)
        {
            renderFinishedSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
        }
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            presentCompleteSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
            inFlightFences.emplace_back(device, vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled});
        }
    }

    // -------------------------------------------------------------------------
    // Members
    // -------------------------------------------------------------------------

    GLFWwindow*                          window                    = nullptr;
    vk::raii::Context                    context;
    vk::raii::Instance                   instance                  = nullptr;
    vk::raii::DebugUtilsMessengerEXT     debugMessenger            = nullptr;
    vk::raii::SurfaceKHR                 surface                   = nullptr;
    vk::raii::PhysicalDevice             physicalDevice            = nullptr;
    vk::raii::Device                     device                    = nullptr;
    vk::raii::Queue                      queue                     = nullptr;
    uint32_t                             graphicsIndex             = 0;
    vk::raii::SwapchainKHR               swapChain                 = nullptr;
    std::vector<vk::Image>               swapChainImages;
    vk::SurfaceFormatKHR                 swapChainSurfaceFormat;
    vk::Extent2D                         swapChainExtent;
    std::vector<vk::raii::ImageView>     swapChainImageViews;
    vk::raii::PipelineLayout             pipelineLayout            = nullptr;
    vk::raii::Pipeline                   graphicsPipeline          = nullptr;
    vk::raii::CommandPool                commandPool               = nullptr;
    std::vector<vk::raii::CommandBuffer> commandBuffers;
    std::vector<vk::raii::Semaphore>     presentCompleteSemaphores;
    std::vector<vk::raii::Semaphore>     renderFinishedSemaphores;
    std::vector<vk::raii::Fence>         inFlightFences;
    uint32_t                             frameIndex                = 0;
    bool                                 framebufferResized        = false;
    std::vector<const char*>             requiredDeviceExtensions  = {vk::KHRSwapchainExtensionName};
};

int main()
{
    try
    {
        HelloTriangleApplication app;
        app.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
