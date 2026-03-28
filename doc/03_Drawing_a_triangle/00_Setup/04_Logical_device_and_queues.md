# 逻辑设备与队列（Logical Device and Queues）

> 原文：[https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/00_Setup/04_Logical_device_and_queues.html](https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/00_Setup/04_Logical_device_and_queues.html)

## 简介（Introduction）

在选定物理设备之后，需要设置一个**逻辑设备**（logical device）来与之交互。逻辑设备的创建过程与 instance 的创建类似，需要描述你希望使用的功能。你可以从同一个物理设备创建多个逻辑设备，每个设备有不同的需求。

第一步是添加一个类成员来存储逻辑设备句柄：

```cpp
vk::raii::Device device = nullptr;
```

接下来，添加一个 `createLogicalDevice` 函数，并在 `initVulkan` 中调用：

```cpp
void initVulkan() {
    createInstance();
    setupDebugMessenger();
    pickPhysicalDevice();
    createLogicalDevice();
}

void createLogicalDevice() {

}
```

## 指定要创建的队列（Specifying the Queues to be Created）

创建逻辑设备需要通过一系列结构体来指定详细信息，首先是 `vk::DeviceQueueCreateInfo`，它描述了针对单个 queue family 所需的队列数量。目前只需要一个支持 graphics 操作的队列：

```cpp
std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
auto graphicsQueueFamilyProperty = std::ranges::find_if(queueFamilyProperties, [](auto const &qfp) { return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0); });
auto graphicsIndex = static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), graphicsQueueFamilyProperty));
vk::DeviceQueueCreateInfo deviceQueueCreateInfo { .queueFamilyIndex = graphicsIndex };
```

当前驱动程序每个 queue family 只允许创建少量队列。可以在多个线程上创建 command buffer，然后在主线程上通过一次低开销的调用统一提交。

Vulkan 允许使用 `0.0` 到 `1.0` 之间的浮点数为队列分配优先级，以影响 command buffer 的执行调度。即便只有一个队列，此优先级也是必须指定的：

```cpp
float queuePriority = 0.5f;
vk::DeviceQueueCreateInfo deviceQueueCreateInfo { .queueFamilyIndex = graphicsIndex, .queueCount = 1, .pQueuePriorities = &queuePriority };
```

## 指定使用的设备特性（Specifying Used Device Features）

定义要使用的设备特性集合。这些特性之前已通过 `vk::raii::PhysicalDevice::getFeatures` 查询过。目前暂时不需要任何特殊特性：

```cpp
vk::PhysicalDeviceFeatures deviceFeatures;
```

## 启用额外的设备特性（Enabling Additional Device Features）

Vulkan 通过默认只提供 Vulkan 1.0 特性来维持向后兼容性。更新的特性必须在设备创建时显式请求。

特性按引入版本或功能分类组织到不同的结构体中：
- 基础特性位于 `vk::PhysicalDeviceFeatures`
- Vulkan 1.3 特性位于 `vk::PhysicalDeviceVulkan13Features`
- 特定扩展的特性有其专属结构体

Vulkan 使用"结构体链"（structure chaining）来启用多个特性集合。每个特性结构体都包含一个 `pNext` 字段，指向下一个结构体。C++ API 提供了 `vk::StructureChain` 来简化这一操作：

```cpp
vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT> featureChain = {
    {},                               // vk::PhysicalDeviceFeatures2（暂时留空）
    {.dynamicRendering = true },      // 启用来自 Vulkan 1.3 的 dynamic rendering
    {.extendedDynamicState = true }   // 启用来自扩展的 extended dynamic state
};
```

`vk::StructureChain` 模板通过自动设置 `pNext` 指针来连接各结构体，从而避免了手动链接的繁琐。

## 指定设备扩展（Specifying Device Extensions）

应用程序正常运行需要某些 device 扩展：

```cpp
std::vector<const char*> requiredDeviceExtension = {
    vk::KHRSwapchainExtensionName};
```

`"VK_KHR_swapchain"` 扩展是将渲染图像呈现到窗口所必需的。

## 创建逻辑设备（Creating the Logical Device）

准备好必要信息后，填写 `vk::DeviceCreateInfo` 并连接特性链，以创建逻辑设备：

```cpp
vk::DeviceCreateInfo deviceCreateInfo{
    .pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
    .queueCreateInfoCount = 1,
    .pQueueCreateInfos = &deviceQueueCreateInfo,
    .enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtension.size()),
    .ppEnabledExtensionNames = requiredDeviceExtension.data()
};
```

特性链的连接方式如下：
1. `featureChain.get<vk::PhysicalDeviceFeatures2>()` 获取第一个结构体
2. 该引用被赋值给 `deviceCreateInfo` 中的 `pNext`
3. Vulkan 沿 `pNext` 链遍历以获取所有请求的特性

`vk::DeviceCreateInfo` 中的 `enabledExtensionCount` 和 `ppEnabledLayerNames` 字段在当前实现中会被忽略。像 `VK_KHR_swapchain` 这样的 device 扩展允许将该设备渲染的图像呈现到窗口。某些设备可能不具备此能力。

```cpp
device = vk::raii::Device( physicalDevice, deviceCreateInfo );
```

此调用接受物理设备和使用信息作为参数。若启用了不存在的扩展或指定了不支持的特性，则可能抛出错误。

## 获取队列句柄（Retrieving Queue Handles）

队列会在逻辑设备创建时自动创建，但需要获取句柄才能与之交互。为 graphics queue 句柄添加类成员：

```cpp
vk::raii::Queue graphicsQueue;
```

Device queue 在设备销毁时会被隐式清理。使用 `vk::raii::Queue` 构造函数来获取句柄，需要指定逻辑设备、queue family 索引以及队列索引：

```cpp
graphicsQueue = vk::raii::Queue( device, graphicsIndex, 0 );
```

有了逻辑设备和队列句柄，就可以开始使用显卡进行操作了。后续章节将介绍如何设置资源，以将结果呈现到窗口系统。
