# 校验层（Validation Layers）

> 原文：[https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/00_Setup/02_Validation_layers.html](https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/00_Setup/02_Validation_layers.html)

## 什么是 validation layer？

Vulkan API 的设计理念是尽量减少驱动程序开销，这一目标的直接体现之一就是：默认情况下 API 的错误检查极为有限。即便是将枚举值设置为错误的值，或向必要参数传入空指针这样简单的错误，通常也不会被显式处理（除 C++ 类型检查外），而只会导致崩溃或未定义行为。由于 Vulkan 要求你对所做的每一件事都非常明确，因此很容易犯许多小错误，例如使用了某个新的 GPU 特性，却忘记在创建逻辑设备时请求它。

然而，这并不意味着这些检查不能添加到 API 中。Vulkan 为此引入了一套优雅的机制，称为 **validation layer**。Validation layer 是可选的组件，它们钩入 Vulkan 函数调用，以执行额外的操作。Validation layer 中常见的操作包括：

- 对照规范检查参数值，以检测误用
- 追踪对象的创建和销毁，以发现资源泄漏
- 通过追踪调用来源的线程来检查线程安全性
- 将每次调用及其参数记录到标准输出
- 追踪 Vulkan 调用，用于性能分析和回放

以下是一个诊断性 validation layer 中某个函数的实现示例：

```cpp
VkResult vkCreateInstance(
    const VkInstanceCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkInstance* instance) {

    if (pCreateInfo == nullptr || instance == nullptr) {
        log("Null pointer passed to required parameter!");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return real_vkCreateInstance(pCreateInfo, pAllocator, instance);
}
```

这些 validation layer 可以自由叠加，以包含所有你感兴趣的调试功能。你可以在 debug 构建中启用 validation layer，在 release 构建中完全禁用，从而两全其美！

Vulkan 本身不内置任何 validation layer，但 LunarG Vulkan SDK 提供了一套出色的 layer，用于检查常见错误。这些 layer 也是完全开源的，你可以查看它们检查哪些类型的错误，并做出贡献。使用 validation layer 是避免应用程序因意外依赖未定义行为而在不同驱动程序上崩溃的最佳方式。

Validation layer 只有在系统上已安装时才能使用。例如，LunarG validation layer 只在安装了 Vulkan SDK 的 PC 上可用。

此外还有其他 layer，我们推荐加以利用。我们将留给读者自行探索和尝试其他 layer。不过，我们推荐关注 BestPractices，它提供了关于如何以最新方式使用 Vulkan 的更多帮助和建议。

Vulkan 过去有两种不同类型的 validation layer：instance 级别和 device 级别。其设计初衷是：instance layer 只检查与全局 Vulkan 对象（如 instance）相关的调用，device-specific layer 只检查与特定 GPU 相关的调用。Device-specific layer 现已被弃用，这意味着 instance validation layer 适用于所有 Vulkan 调用。

## 使用 validation layer（Using Validation Layers）

在本节中，我们将了解如何启用 Vulkan SDK 提供的标准诊断 layer。与扩展一样，validation layer 也需要通过指定其名称来启用。所有有用的标准校验功能都被打包在 SDK 中的一个 layer 中，即 `VK_LAYER_KHRONOS_validation`。

首先，在程序中添加两个配置变量，分别指定要启用的 layer 以及是否启用它们。我们选择根据程序是否以 debug 模式编译来决定该值。`NDEBUG` 宏是 C++ 标准的一部分，意为"非调试"（not debug）。

```cpp
constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;

const std::vector<char const*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif
```

我们需要检查所有请求的 layer 是否可用。需要遍历请求的 layer，并验证所有必要的 layer 均受 Vulkan 实现支持。该检查直接在 `createInstance` 函数中执行：

```cpp
void createInstance()
{
    ...

    // 获取所需的 layer
    std::vector<char const*> requiredLayers;
    if (enableValidationLayers)
    {
        requiredLayers.assign(validationLayers.begin(), validationLayers.end());
    }

    // 检查所需的 layer 是否受 Vulkan 实现支持
    auto layerProperties = context.enumerateInstanceLayerProperties();
    auto unsupportedLayerIt = std::ranges::find_if(requiredLayers,
                                                   [&layerProperties](auto const &requiredLayer) {
                                                       return std::ranges::none_of(layerProperties,
                                                                                   [requiredLayer](auto const &layerProperty) { return strcmp(layerProperty.layerName, requiredLayer) == 0; });
                                                   });
    if (unsupportedLayerIt != requiredLayers.end())
    {
        throw std::runtime_error("Required layer not supported: " + std::string(*unsupportedLayerIt));
    }

    ...
}
```

## 使用扩展（Using Extensions）

如上所述，扩展也需要通过名称来启用。但这里需要区分 instance 扩展和 device 扩展。

我们首先创建一个 `getRequiredInstanceExtensions` 函数，返回所需的 instance 扩展列表：

```cpp
std::vector<const char*> getRequiredInstanceExtensions()
{
    uint32_t glfwExtensionCount = 0;
    auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    return extensions;
}
```

GLFW 指定的扩展始终是必需的，因为我们使用 GLFW 依赖项来处理窗口。

我们需要检查所有必需的扩展是否可用。首先使用 `vk::raii::Context::enumerateInstanceLayerProperties` 函数获取所有受支持的 instance 扩展列表，然后检查所有必需的 layer 是否都在该列表中。该检查同样直接在 `createInstance` 函数中执行：

```cpp
void createInstance()
{
    ...

    // 获取所需的扩展
    auto requiredExtensions = getRequiredInstanceExtensions();

    // 检查所需的扩展是否受 Vulkan 实现支持
    auto extensionProperties = context.enumerateInstanceExtensionProperties();
    auto unsupportedPropertyIt =
        std::ranges::find_if(requiredExtensions,
                             [&extensionProperties](auto const &requiredExtension) {
                                 return std::ranges::none_of(extensionProperties,
                                                             [requiredExtension](auto const &extensionProperty) { return strcmp(extensionProperty.extensionName, requiredExtension) == 0; });
                             });
    if (unsupportedPropertyIt != requiredExtensions.end())
    {
        throw std::runtime_error("Required extension not supported: " + std::string(*unsupportedPropertyIt));
    }

    ...
}
```

现在以 debug 模式运行程序，确保不会出现错误。如果出现错误，请参阅 FAQ。

最后，修改 `vk::InstanceCreateInfo` 结构体的实例化，加入 validation layer 名称和扩展名称：

```cpp
void createInstance()
{
    ...

    vk::InstanceCreateInfo createInfo{.pApplicationInfo        = &appInfo,
                                      .enabledLayerCount       = static_cast<uint32_t>(requiredLayers.size()),
                                      .ppEnabledLayerNames     = requiredLayers.data(),
                                      .enabledExtensionCount   = static_cast<uint32_t>(requiredExtensions.size()),
                                      .ppEnabledExtensionNames = requiredExtensions.data()};
    instance = vk::raii::Instance(context, createInfo);
}
```

如果检查通过，`vk::raii::Instance` 的构造函数就不应该再抛出 `vk::Result::eErrorLayerNotPresent` 错误，但你仍应运行程序以进行确认。

## 消息回调（Message Callback）

Validation layer 默认将调试消息打印到标准输出，但我们也可以在程序中提供显式的回调来自行处理这些消息。这样还可以让你决定希望看到哪类消息，因为并非所有消息都一定是（致命的）错误。如果你现在不想处理这些，可以跳到本章的最后一节。

要在程序中设置处理消息及其详细信息的回调，需要使用 `VK_EXT_debug_utils` 扩展来设置一个带有回调的 debug messenger。

我们首先根据是否启用了 validation layer，扩展 `getRequiredInstanceExtensions` 函数：

```cpp
std::vector<const char*> getRequiredInstanceExtensions()
{
    uint32_t glfwExtensionCount = 0;
    auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    if (enableValidationLayers)
    {
        extensions.push_back(vk::EXTDebugUtilsExtensionName);
    }

    return extensions;
}
```

debug messenger 扩展是有条件添加的。注意这里使用了 `vk::EXTDebugUtilsExtensionName` 宏，其值等价于字符串字面量 `"VK_EXT_debug_utils"`。使用此宏可以避免拼写错误。

现在来看看 debug 回调函数的样子。添加一个名为 `debugCallback` 的新静态成员函数，其原型为 `PFN_vkDebugUtilsMessengerCallbackEXT`。`VKAPI_ATTR` 和 `VKAPI_CALL` 确保函数具有正确的签名以供 Vulkan 调用。

```cpp
static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT       severity,
                                                      vk::DebugUtilsMessageTypeFlagsEXT              type,
                                                      const vk::DebugUtilsMessengerCallbackDataEXT * pCallbackData,
                                                      void *                                         pUserData)
{
  std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;

  return vk::False;
}
```

第一个参数指定消息的严重性级别，可以是以下标志之一：

- `vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose`：来自 Vulkan 组件（如 loader、layer 和驱动程序）的诊断消息
- `vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo`：信息性消息，例如资源的创建
- `vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning`：关于某种行为的消息，该行为不一定是错误，但很可能是应用程序中的 bug
- `vk::DebugUtilsMessageSeverityFlagBitsEXT::eError`：关于无效行为且可能导致崩溃的消息

这个枚举值的设计使得你可以用比较运算来检查消息是否达到或超过某个严重性级别，例如：

```cpp
if (messageSeverity >= vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning) {
    // 消息重要到需要显示
}
```

`messageType` 参数可以取以下值：

- `vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral`：发生了与规范或性能无关的事件
- `vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation`：发生了违反规范或可能存在错误的情况
- `vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance`：潜在的非最优 Vulkan 使用方式

`pCallbackData` 参数指向一个 `vk::DebugUtilsMessengerCallbackDataEXT` 结构体，包含消息本身的详细信息，其中最重要的成员有：

- `pMessage`：调试消息，以 null 结尾的字符串形式
- `pObjects`：与消息相关的 Vulkan 对象句柄数组
- `objectCount`：数组中的对象数量

最后，`pUserData` 参数包含一个在设置回调时指定的指针，允许你将自己的数据传递给回调函数。

回调函数返回一个布尔值，指示触发 validation layer 消息的 Vulkan 调用是否应该被中止。如果回调返回 `true`，则该调用将以 `vk::Result::eErrorValidationFailedEXT` 错误中止。通常这只用于测试 validation layer 本身，因此你应始终返回 `vk::False`。

现在还剩下告知 Vulkan 回调函数的步骤。这样的回调是 **debug messenger** 的一部分，你可以拥有任意数量的 debug messenger。在 `instance` 成员变量下方添加一个类成员用于存储其句柄：

```cpp
vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;
```

现在添加一个 `setupDebugMessenger` 函数，在 `initVulkan` 中紧接 `createInstance` 之后调用：

```cpp
void initVulkan()
{
    createInstance();
    setupDebugMessenger();
}

void setupDebugMessenger()
{
    if (!enableValidationLayers) return;

}
```

我们需要填写一个结构体，包含 messenger 及其回调的详细信息：

```cpp
void setupDebugMessenger()
{
    ...
    vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                                        vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
    vk::DebugUtilsMessageTypeFlagsEXT     messageTypeFlags(
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
    vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT{.messageSeverity = severityFlags,
                                                                          .messageType     = messageTypeFlags,
                                                                          .pfnUserCallback = &debugCallback};
    debugMessenger = instance.createDebugUtilsMessengerEXT( debugUtilsMessengerCreateInfoEXT );
}
```

`messageSeverity` 字段允许你指定希望触发回调的所有严重性类型。我们只关心那些提示应用程序存在问题并可能需要修复的消息，因此请求了 `eWarning` 和 `eError` 标志。

类似地，`messageType` 字段允许你过滤回调所接收到的消息类型。我们在此启用了所有类型。如果某些类型对你没有用，随时可以禁用。

最后，`pfnUserCallback` 字段指定回调函数的指针。你还可以选择向 `pUserData` 字段传入一个指针，该指针将通过 `pUserData` 参数传递给回调函数。例如，你可以用它来传递 `HelloTriangleApplication` 类的指针。

需要注意的是，配置 validation layer 消息和 debug 回调的方式还有很多，但对于本教程来说，这是一个不错的起始配置。有关更多可能的配置，请参阅扩展规范。

完整的 `createInstance` 函数现在如下所示：

```cpp
void createInstance()
{
    constexpr vk::ApplicationInfo appInfo{ .pApplicationName   = "Hello Triangle",
                .applicationVersion = VK_MAKE_VERSION( 1, 0, 0 ),
                .pEngineName        = "No Engine",
                .engineVersion      = VK_MAKE_VERSION( 1, 0, 0 ),
                .apiVersion         = vk::ApiVersion14 };

    // 获取所需的 layer
    std::vector<char const*> requiredLayers;
    if (enableValidationLayers) {
        requiredLayers.assign(validationLayers.begin(), validationLayers.end());
    }

    // 检查所需的 layer 是否受 Vulkan 实现支持
    auto layerProperties = context.enumerateInstanceLayerProperties();
    if (std::ranges::any_of(requiredLayers, [&layerProperties](auto const& requiredLayer) {
        return std::ranges::none_of(layerProperties,
                                   [requiredLayer](auto const& layerProperty)
                                   { return strcmp(layerProperty.layerName, requiredLayer) == 0; });
    }))
    {
        throw std::runtime_error("One or more required layers are not supported!");
    }

    // 获取所需的扩展
    auto requiredExtensions = getRequiredInstanceExtensions();

    // 检查所需的扩展是否受 Vulkan 实现支持
    auto extensionProperties = context.enumerateInstanceExtensionProperties();
    auto unsupportedPropertyIt =
        std::ranges::find_if(requiredExtensions,
                             [&extensionProperties](auto const &requiredExtension) {
                                 return std::ranges::none_of(extensionProperties,
                                                             [requiredExtension](auto const &extensionProperty) { return strcmp(extensionProperty.extensionName, requiredExtension) == 0; });
                             });
    if (unsupportedPropertyIt != requiredExtensions.end())
    {
        throw std::runtime_error("Required extension not supported: " + std::string(*unsupportedPropertyIt));
    }

    vk::InstanceCreateInfo createInfo{
        .pApplicationInfo        = &appInfo,
        .enabledLayerCount       = static_cast<uint32_t>(requiredLayers.size()),
        .ppEnabledLayerNames     = requiredLayers.data(),
        .enabledExtensionCount   = static_cast<uint32_t>(requiredExtensions.size()),
        .ppEnabledExtensionNames = requiredExtensions.data() };
    instance = vk::raii::Instance(context, createInfo);
}
```

## 配置（Configuration）

Validation layer 的行为配置选项远不止 `vk::DebugUtilsMessengerCreateInfoEXT` 结构体中指定的标志。浏览到 Vulkan SDK 目录，进入 `Config` 目录，你会找到一个 `vk_layer_settings.txt` 文件，其中解释了如何配置各 layer。

要为你自己的应用程序配置 layer 设置，请将该文件复制到项目的 `Debug` 和 `Release` 目录，并按照说明设置所需的行为。但在本教程的其余部分，我们将假设你使用默认设置。

在整个教程过程中，我们会故意犯几个错误，以向你展示 validation layer 在捕获这些错误时有多么有帮助，并教会你了解自己在使用 Vulkan 时究竟在做什么有多重要。现在是时候来看看系统中的 Vulkan 设备了。
