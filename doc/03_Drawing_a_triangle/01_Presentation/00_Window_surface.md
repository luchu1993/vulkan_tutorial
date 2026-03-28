# 窗口表面（Window Surface）

> 原文：[https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/01_Presentation/00_Window_surface.html](https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/01_Presentation/00_Window_surface.html)

由于 Vulkan 是一个平台无关的 API，它本身并不能直接与窗口系统交互。为了在 Vulkan 与窗口系统之间建立连接，以便将渲染结果呈现到屏幕上，我们需要使用 WSI（Window System Integration，窗口系统集成）扩展。在本章中，我们将首先讨论 `VK_KHR_surface`，它暴露出一个 `VkSurfaceKHR` 对象，用于表示一个用于呈现渲染图像的抽象表面。这个表面将由我们已经通过 GLFW 打开的窗口来支撑。

`VK_KHR_surface` 扩展是一个实例级别的扩展，实际上我们已经启用了它，因为它包含在 `glfwGetRequiredInstanceExtensions` 返回的列表中。

窗口表面需要在实例创建之后立即创建，因为它实际上可能会影响物理设备的选择。我们之所以将这一部分推迟到现在，是因为窗口表面是一个较大主题的一部分，而过早引入它会打乱基本设置的讲解思路。还需要注意的是，窗口表面在 Vulkan 中完全是可选的，如果你只需要离屏渲染，不像 OpenGL，Vulkan 允许你在不创建可见窗口的情况下完成渲染。

## 创建窗口表面（Window Surface Creation）

首先在 debug callback 成员变量下方添加一个 `surface` 类成员：

```cpp
vk::raii::SurfaceKHR surface = nullptr;
```

虽然 `VkSurfaceKHR` 对象及其使用是平台无关的，但它的创建并不是，因为它依赖于窗口系统的具体细节。例如，在 Windows 上需要使用 `HWND` 和 `HINSTANCE` 句柄。因此该扩展有一个平台特定的附加项，在 Windows 上叫做 `VK_KHR_win32_surface`，它也自动包含在 `glfwGetRequiredInstanceExtensions` 的返回列表中。

让我们来演示一下这个平台特定的扩展如何在 Windows 上用于创建表面，但在本教程中我们实际上不会使用这种方式。使用像 GLFW 这样的库并在它能力范围之外执行操作是没有任何意义的。GLFW 实际上有一个 `glfwCreateWindowSurface` 函数，它为我们处理了各平台的差异。尽管如此，在我们开始依赖它之前，先了解一下它背后的工作原理是有益的。

要访问原生平台函数，你需要更新文件顶部的头文件包含：

```cpp
#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
```

由于窗口表面是一个 Vulkan 对象，它附带了一个需要填写的 `vk::Win32SurfaceCreateInfoKHR` 结构体。它有两个重要参数：`hinstance` 和 `hwnd`，分别对应进程和窗口。

```cpp
vk::Win32SurfaceCreateInfoKHR createInfo{.hinstance = GetModuleHandle(nullptr),
                                         .hwnd      = glfwGetWin32Window(window)};
```

`glfwGetWin32Window` 函数用于从 GLFW 窗口对象获取原始的 `HWND`。`GetModuleHandle` 调用返回当前进程的 `HINSTANCE` 句柄。

之后便可以使用 raii 方式创建表面：

```cpp
surface = instance.createWin32SurfaceKHR(createInfo);
```

在 Linux 等其他平台上过程类似，其中 `vk::raii::Instance::createXcbSurfaceKHR` 以 XCB 连接和窗口作为创建细节，与 X11 配合使用。

`glfwCreateWindowSurface` 函数对每个平台使用不同的实现来完成完全相同的操作。现在我们将把它集成到我们的程序中。添加一个 `createSurface` 函数，在 `initVulkan` 中于实例创建和 `setupDebugMessenger` 之后立即调用：

```cpp
void initVulkan() {
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
}

void createSurface() {

}
```

GLFW 的调用非常简单：

```cpp
void createSurface() {
    VkSurfaceKHR       _surface;
    if (glfwCreateWindowSurface(*instance, window, nullptr, &_surface) != 0) {
        throw std::runtime_error("failed to create window surface!");
    }
    surface = vk::raii::SurfaceKHR(instance, _surface);
}
```

不过，正如你在上面所看到的，GLFW 只处理 Vulkan 的 C API。`VkSurfaceKHR` 对象是一个 C API 对象。好在它可以原生地提升为 C++ 封装器，这正是我们在这里所做的。

参数依次为：`VkInstance`、GLFW 窗口指针、自定义分配器，以及指向 `VkSurfaceKHR` 变量的指针。GLFW 没有提供专门用于销毁表面的函数，但将其封装在我们的 raii `SurfaceKHR` 对象中后，Vulkan RAII 会为我们自动处理销毁工作。

## 查询呈现支持（Querying for Presentation Support）

尽管 Vulkan 实现可能支持窗口系统集成，但这并不意味着系统中的每个设备都支持它。因此，我们需要扩展 `createLogicalDevice`，以确保某个设备能够将图像呈现到我们创建的表面上。

由于呈现是一个队列相关的特性，问题实际上是找到一个支持向我们创建的表面呈现图像的 queue family。

支持图形命令的 queue family 与支持呈现的 queue family 实际上有可能不重叠，但这种情况极为罕见。

Queue family 搜索逻辑如下：

```cpp
uint32_t queueIndex = ~0;
for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++)
{
  if ((queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) &&
      physicalDevice.getSurfaceSupportKHR(qfpIndex, *surface))
  {
    // found a queue family that supports both graphics and present
    queueIndex = qfpIndex;
    break;
  }
}
if (queueIndex == ~0)
{
  throw std::runtime_error("Could not find a queue for graphics and present -> terminating");
}
```

用于检查呈现支持的函数是 `vk::raii::PhysicalDevice::getSurfaceSupportKHR`，它以 queue family 索引和 surface 作为参数。

## 创建呈现队列（Creating the Presentation Queue）

剩下的工作是修改逻辑设备的创建过程，以创建队列并取回 `vk::raii::Queue` 句柄。

我们需要修改过滤逻辑，以便在检测时找到最合适的 queue family 来使用。

完整实现如下：

```cpp
void createLogicalDevice() {
    // find the index of the first queue family that supports graphics
    std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

    // get the first index into queueFamilyProperties which supports both graphics and present
    uint32_t queueIndex = ~0;
    for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++)
    {
      if ((queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) &&
          physicalDevice.getSurfaceSupportKHR(qfpIndex, *surface))
      {
        // found a queue family that supports both graphics and present
        queueIndex = qfpIndex;
        break;
      }
    }
    if (queueIndex == ~0)
    {
      throw std::runtime_error("Could not find a queue for graphics and present -> terminating");
    }

    // query for Vulkan 1.3 features
    vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT> featureChain = {
        {},                                   // vk::PhysicalDeviceFeatures2
        {.dynamicRendering = true},           // vk::PhysicalDeviceVulkan13Features
        {.extendedDynamicState = true}        // vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
    };

    // create a Device
    float                     queuePriority = 0.5f;
    vk::DeviceQueueCreateInfo deviceQueueCreateInfo{.queueFamilyIndex = queueIndex, .queueCount = 1, .pQueuePriorities = &queuePriority};
    vk::DeviceCreateInfo      deviceCreateInfo{.pNext                   = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
                                               .queueCreateInfoCount    = 1,
                                               .pQueueCreateInfos       = &deviceQueueCreateInfo,
                                               .enabledExtensionCount   = static_cast<uint32_t>(requiredDeviceExtension.size()),
                                               .ppEnabledExtensionNames = requiredDeviceExtension.data()};

    device = vk::raii::Device( physicalDevice, deviceCreateInfo );
    queue  = vk::raii::Queue(device, queueIndex, 0);
}
```

在[下一章](01_Swap_chain.html)中，我们将研究 swap chain 以及它如何让我们将图像呈现到表面上。
