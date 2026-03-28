# 交换链（Swap Chain）

> 原文：[https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/01_Presentation/01_Swap_chain.html](https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/01_Presentation/01_Swap_chain.html)

Vulkan 没有"默认 framebuffer"的概念，因此它需要一种基础设施来持有我们在将结果显示到屏幕之前渲染所用的缓冲区。这种基础设施被称为 **swap chain**，必须在 Vulkan 中显式创建。swap chain 本质上是一个等待呈现到屏幕的图像队列。我们的应用程序会获取这样一张图像来进行绘制，然后将其返回到队列中。队列的工作方式以及从队列中呈现图像的条件取决于 swap chain 的配置方式，但 swap chain 的总体目的是使图像的呈现与屏幕的刷新率同步。

## 检查 swap chain 支持（Checking for Swap Chain Support）

并非所有显卡都能够直接将图像呈现到屏幕，例如，为服务器设计的显卡没有任何显示输出。其次，由于图像呈现与窗口系统以及与窗口关联的表面密切相关，它实际上并不是 Vulkan 核心的一部分。你必须在查询支持后启用 `VK_KHR_swapchain` 设备扩展。

为此，我们首先扩展 `pickPhysicalDevice` 函数以检查该扩展是否受支持。我们之前已经了解了如何列出 `vk::PhysicalDevice` 支持的扩展，所以实现起来应该相当简单。请注意，Vulkan 头文件提供了一个很好的宏 `VK_KHR_SWAPCHAIN_EXTENSION_NAME`，它被定义为 `VK_KHR_swapchain`。使用这个宏的优点是编译器能够捕获拼写错误。

声明所需设备扩展的列表，类似于要启用的 validation layer 列表：

```cpp
std::vector<const char*> requiredDeviceExtension = {
    vk::KHRSwapchainExtensionName};
```

## 启用设备扩展（Enabling Device Extensions）

使用 swapchain 需要先启用 `VK_KHR_swapchain` 扩展。启用该扩展只需对逻辑设备的创建结构做一个小改动：

```cpp
std::vector requiredDeviceExtension = { vk::KHRSwapchainExtensionName };

float                     queuePriority = 0.5f;
vk::DeviceQueueCreateInfo deviceQueueCreateInfo{.queueFamilyIndex = queueIndex, .queueCount = 1, .pQueuePriorities = &queuePriority};
vk::DeviceCreateInfo      deviceCreateInfo{.pNext                   = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
                                           .queueCreateInfoCount    = 1,
                                           .pQueueCreateInfos       = &deviceQueueCreateInfo,
                                           .enabledExtensionCount   = static_cast<uint32_t>(requiredDeviceExtension.size()),
                                           .ppEnabledExtensionNames = requiredDeviceExtension.data()};
```

## 查询 swap chain 支持细节（Querying Swap Chain Support Details）

仅仅检查 swap chain 是否可用还不够，因为它可能与我们的窗口表面实际上并不兼容。创建 swap chain 还涉及比创建实例和设备更多的设置，因此在我们能够继续之前，需要查询更多细节。

基本上有三类需要检查的属性：

- 基本表面能力（surface capabilities）：swap chain 中图像的最小/最大数量、图像的最小/最大宽高
- 表面格式（surface formats）：像素格式、色彩空间
- 可用的呈现模式（presentation modes）

查询表面能力：

```cpp
auto surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR( *surface );
```

查询支持的表面格式：

```cpp
std::vector<vk::SurfaceFormatKHR> availableFormats = physicalDevice.getSurfaceFormatsKHR( surface );
```

查询支持的呈现模式：

```cpp
std::vector<vk::PresentModeKHR> availablePresentModes = physicalDevice.getSurfacePresentModesKHR( surface );
```

对于本教程而言，如果在给定窗口表面的条件下至少有一种支持的图像格式和一种支持的呈现模式，swap chain 的支持就已经足够了。

## 为 swap chain 选择合适的设置（Choosing the Right Settings for the Swap Chain）

如果满足了前面章节中的条件，那么支持肯定是足够的，但可能仍然存在许多不同的优化模式。我们现在将编写几个函数来找到最佳 swap chain 的正确设置。

需要确定三类设置：

- Surface format（颜色深度）
- Presentation mode（将图像"交换"到屏幕的条件）
- Swap extent（swap chain 中图像的分辨率）

对于这些设置中的每一个，我们都会有一个理想值，如果它可用就使用它，否则我们将回退到某个次优选项。

### 表面格式（Surface Format）

```cpp
vk::SurfaceFormatKHR chooseSwapSurfaceFormat(std::vector<vk::SurfaceFormatKHR> const &availableFormats)
{
    assert(!availableFormats.empty());
    return availableFormats[0];
}
```

每个 `vk::SurfaceFormatKHR` 条目包含一个 `format` 和一个 `colorSpace` 成员。`format` 成员指定颜色通道和类型。例如，`vk::Format::eB8G8R8A8Srgb` 表示我们按 B、G、R 和 alpha 的顺序存储颜色通道，每个通道使用 8 位无符号整数，总计每像素 32 位。`colorSpace` 成员通过 `vk::ColorSpaceKHR::eSrgbNonlinear` 标志指示是否支持 SRGB 色彩空间。

对于色彩空间，我们将使用 SRGB（如果可用），因为它能[更准确地感知颜色](http://stackoverflow.com/questions/12524623/)。它也是图像（如纹理）最常用的标准色彩空间。正因如此，我们也应该使用 SRGB 颜色格式，其中最常见的一个是 `vk::Format::eB8G8R8A8Srgb`。

```cpp
vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats) {
    const auto formatIt = std::ranges::find_if(
        availableFormats,
        [](const auto &format) { return format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear; });
    return formatIt != availableFormats.end() ? *formatIt : availableFormats[0];
}
```

如果找不到理想格式，我们可以对可用格式进行排名，从"最好的"开始选择，但在大多数情况下，直接使用第一个可用格式也是可以接受的。

### 呈现模式（Presentation Mode）

呈现模式可以说是 swap chain 最重要的设置，因为它代表了向屏幕显示图像的实际条件。Vulkan 中有四种可能的模式：

- `vk::PresentModeKHR::eImmediate`：应用程序提交的图像会立即传输到屏幕，这可能会导致画面撕裂。

- `vk::PresentModeKHR::eFifo`：swap chain 是一个队列，显示器在刷新时从队列前端取走图像，程序将渲染好的图像插入到队列末尾。如果队列已满，程序必须等待。这与现代游戏中的垂直同步最为类似。显示刷新的那一刻被称为"垂直空白期（vertical blank）"。

- `vk::PresentModeKHR::eFifoRelaxed`：该模式与前一种模式的区别仅在于：如果应用程序延迟，并且在上一个垂直空白期时队列为空，图像将在最终到达时立即传输，而不是等待下一个垂直空白期。这可能会导致可见的撕裂。

- `vk::PresentModeKHR::eMailbox`：这是第二种模式的另一个变体。当队列已满时，不会阻塞应用程序，而是直接用更新的图像替换已经排队的图像。这种模式可以用来尽可能快地渲染帧，同时避免撕裂，与垂直同步相比延迟更低。这通常被称为"三重缓冲（triple buffering）"，尽管仅有三个缓冲区的存在并不一定意味着帧率是解锁的。

只有 `eFifo` 模式是保证可用的：

```cpp
vk::PresentModeKHR chooseSwapPresentMode(std::vector<vk::PresentModeKHR> const &availablePresentModes)
{
    return vk::PresentModeKHR::eFifo;
}
```

我个人认为，如果能耗不是问题，`eMailbox` 是一个非常好的折衷方案。它使我们能够在避免撕裂的同时，通过在垂直空白期之前渲染尽可能新的帧来保持较低的延迟。在移动设备上，能耗更为重要，这时使用 `eFifo` 可能更为合适。

```cpp
vk::PresentModeKHR chooseSwapPresentMode(std::vector<vk::PresentModeKHR> const &availablePresentModes)
{
    assert(std::ranges::any_of(availablePresentModes, [](auto presentMode) { return presentMode == vk::PresentModeKHR::eFifo; }));
    return std::ranges::any_of(availablePresentModes,
                               [](const vk::PresentModeKHR value) { return vk::PresentModeKHR::eMailbox == value; }) ?
               vk::PresentModeKHR::eMailbox :
               vk::PresentModeKHR::eFifo;
}
```

### 交换范围（Swap Extent）

```cpp
vk::Extent2D chooseSwapExtent(vk::SurfaceCapabilitiesKHR const &capabilities)
{}
```

swap extent 是 swap chain 图像的分辨率，它几乎总是与我们绘制目标窗口的分辨率完全相同（以像素为单位）。可能的分辨率范围由 `vk::SurfaceCapabilitiesKHR` 结构体定义。

Vulkan 告诉我们通过设置 `currentExtent` 成员中的宽度和高度来匹配窗口的分辨率。然而，某些窗口管理器允许我们在这里有所不同，这通过将 `currentExtent` 中的宽度和高度设置为一个特殊值来表示：`uint32_t` 的最大值。在这种情况下，我们将在 `minImageExtent` 和 `maxImageExtent` 的范围内选择最匹配窗口的分辨率。但我们必须以正确的单位来指定分辨率。

GLFW 使用两种单位来衡量尺寸：像素和[屏幕坐标](https://www.glfw.org/docs/latest/intro_guide.html#coordinate_systems)。例如，我们之前在创建窗口时指定的分辨率 `{WIDTH, HEIGHT}` 是以屏幕坐标来衡量的。但 Vulkan 使用的是像素，因此 swap chain 的 extent 也必须以像素指定。遗憾的是，如果你使用的是高 DPI 显示器（如 Apple 的 Retina 显示屏），屏幕坐标与像素并不对应。由于像素密度更高，窗口的实际像素分辨率将大于以屏幕坐标表示的分辨率。因此，如果 Vulkan 不自动为我们修正 swap extent，我们就不能仅仅使用原始的 `{WIDTH, HEIGHT}`。而应该使用 `glfwGetFramebufferSize` 来查询窗口的像素分辨率，然后再将其与最小和最大图像 extent 进行比较。

```cpp
#include <cstdint>
#include <limits>
#include <algorithm>

vk::Extent2D chooseSwapExtent(vk::SurfaceCapabilitiesKHR const &capabilities)
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return capabilities.currentExtent;
    }
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    return {
        std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
        std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
    };
}
```

`clamp` 函数用于将 `width` 和 `height` 的值限制在实现所支持的允许的最小和最大 extent 之间。

## 创建 swap chain（Creating the Swap Chain）

现在我们有了所有这些辅助函数来帮助我们在运行时做出选择，我们终于有了创建一个可工作的 swap chain 所需的全部信息。

创建一个 `createSwapChain` 函数，从这些调用开始，并在 `initVulkan` 中的逻辑设备创建之后调用它：

```cpp
void initVulkan() {
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
}

void createSwapChain() {
    vk::SurfaceCapabilitiesKHR surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR( *surface );
    swapChainExtent                                = chooseSwapExtent(surfaceCapabilities);
    uint32_t minImageCount                         = chooseSwapMinImageCount(surfaceCapabilities);

    std::vector<vk::SurfaceFormatKHR> availableFormats = physicalDevice.getSurfaceFormatsKHR(*surface);
    swapChainSurfaceFormat                             = chooseSwapSurfaceFormat(availableFormats);
}
```

除了这些属性之外，我们还必须决定 swap chain 中希望拥有多少张图像。实现过程中会指定它所需的最小数量：

```cpp
uint32_t imageCount = surfaceCapabilities.minImageCount;
```

然而，仅仅坚持这个最小值意味着我们有时可能需要等待驱动程序完成内部操作，才能获取另一张用于渲染的图像。因此，建议请求至少比最小值多一张图像：

```cpp
uint32_t chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const &surfaceCapabilities)
{
    auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
    if ((0 < surfaceCapabilities.maxImageCount) && (surfaceCapabilities.maxImageCount < minImageCount))
    {
        minImageCount = surfaceCapabilities.maxImageCount;
    }
    return minImageCount;
}
```

我们还要确保不超过最大图像数量，其中 `0` 是一个特殊值，表示没有最大值限制。

现在我们可以开始填写 `vk::SwapchainCreateInfoKHR` 结构体了：

```cpp
vk::SwapchainCreateInfoKHR swapChainCreateInfo{.surface          = *surface,
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
                                               .clipped          = true};
```

在指定了 surface 之后，swap chain 图像的数量由 `minImageCount` 指定。

`imageArrayLayers` 指定每张图像由多少层组成。除非你在开发立体 3D 应用程序，否则这个值始终为 `1`。

`imageUsage` 位域指定我们将对 swap chain 中的图像执行哪些操作。在本教程中，我们将直接渲染到它们，这意味着它们被用作颜色附件（color attachment）。如果你想先将图像渲染到一个单独的图像上以执行后处理等操作，也可以使用像 `vk::ImageUsageFlagBits::eTransferDst` 这样的值，然后使用内存操作将渲染后的图像传输到 swap chain 图像。

`imageSharingMode` 指定如何处理可能跨多个 queue family 使用的 swap chain 图像。如果图形 queue family 与呈现 queue family 不同，就会出现这种情况。我们将从图形队列中绘制 swap chain 中的图像，然后在呈现队列上提交它们。处理从多个队列访问图像有两种方式：

- `vk::SharingMode::eExclusive`：图像一次只能被一个 queue family 所有，在另一个 queue family 使用它之前，必须显式转移所有权。该选项提供最佳性能。
- `vk::SharingMode::eConcurrent`：图像可以跨多个 queue family 使用，无需显式的所有权转移。

如果 queue family 不同，本教程将使用 concurrent 模式以避免处理所有权相关内容，因为这涉及一些需要在后续章节中解释的概念。Concurrent 模式要求你通过 `queueFamilyIndexCount` 和 `pQueueFamilyIndices` 参数预先指定将在哪些 queue family 之间共享所有权。如果图形 queue family 和呈现 queue family 相同（在大多数硬件上都是如此），我们应该坚持使用 exclusive 模式，因为 concurrent 模式要求你指定至少两个不同的 queue family。

`preTransform` 指定如果支持的话（`supportedTransforms` 在 `surfaceCapabilities` 中），是否应对 swap chain 中的图像应用某个变换，例如顺时针旋转 90 度或水平翻转。要指定不需要变换，只需指定当前变换即可。

`compositeAlpha` 字段指定 alpha 通道是否应用于与窗口系统中其他窗口的混合。你几乎总是希望简单地忽略 alpha 通道，因此使用 `vk::CompositeAlphaFlagBitsKHR::eOpaque`。

`clipped` 成员设置为 `true` 意味着我们不关心被遮挡的像素的颜色，例如因另一个窗口位于其前面而被遮挡的像素。除非你真的需要读取这些像素并获得可预测的结果，否则启用裁剪可以获得最佳性能。

还有最后一个字段，`oldSwapchain`：

```cpp
swapChainCreateInfo.oldSwapchain = nullptr;
```

在 Vulkan 中，当你的应用程序运行时，swap chain 可能会失效或变得次优，例如因为窗口被调整了大小。在这种情况下，实际上需要从头开始重新创建 swap chain，并且必须在该字段中指定对旧 swap chain 的引用。这是一个复杂的话题，我们将在[以后的章节](../03_Drawing/03_Framebuffers.html)中学习更多。现在我们先假设只会创建一个 swap chain。

添加一个类成员来存储 `vk::raii::SwapchainKHR` 对象以及 swap chain 图像：

```cpp
vk::raii::SwapchainKHR swapChain;
std::vector<vk::Image> swapChainImages;
```

现在创建 swap chain 就像调用 `vk::raii::SwapchainKHR` 构造函数一样简单：

```cpp
swapChain       = vk::raii::SwapchainKHR( device, swapChainCreateInfo );
swapChainImages = swapChain.getImages();
```

参数为逻辑设备和 swap chain 创建信息。

## 获取 swap chain 图像（Retrieving the Swap Chain Images）

swap chain 现已创建完成，剩下的就是获取其中 `vk::Image` 对象的句柄了。我们将在后续章节的渲染操作中引用这些句柄。

```cpp
std::vector<vk::Image> swapChainImages = swapChain->getImages();
```

请注意，图像是由 swap chain 实现创建的，一旦 swap chain 被销毁，它们也将自动清理，因此我们不需要添加任何额外的清理代码。

我在 `createSwapChain` 函数末尾将格式和 extent 存储在成员变量中，因为我们在以后的章节中将需要它们：

```cpp
vk::raii::SwapchainKHR swapChain = nullptr;
std::vector<vk::Image> swapChainImages;
vk::SurfaceFormatKHR   swapChainSurfaceFormat;
vk::Extent2D           swapChainExtent;
```

现在我们有了一组可以绘制并可以呈现到窗口的图像。[下一章](02_Image_views.html)将开始介绍如何将这些图像设置为渲染目标，然后我们将开始研究实际的图形管线和绘制命令！
