# 基础代码（Base Code）

> 原文：[https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/00_Setup/00_Base_code.html](https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/00_Setup/00_Base_code.html)

## Vulkan-hpp 与指派初始化器（Designated Initializers）

本教程使用 C++20 的指派初始化器（designated initializers）。默认情况下，Vulkan-hpp 使用不同的初始化方式，因此需要显式启用 `VULKAN_HPP_NO_STRUCT_CONSTRUCTORS` 宏定义。该定义已在 CMake 构建配置中设置。对于自定义构建系统，需在包含 Vulkan-hpp 头文件之前手动定义：

```cpp
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan.hpp>
// 或
#include <vulkan/vulkan_raii.hpp>
```

## 总体结构（General Structure）

从零开始，基础代码框架如下：

```cpp
#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>

class HelloTriangleApplication {
public:
    void run() {
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    void initVulkan() {

    }

    void mainLoop() {

    }

    void cleanup() {

    }
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
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
```

Vulkan-Hpp 的 RAII 头文件提供了函数、结构体和枚举。通过 CMake 的 `USE_CPP20_MODULES` 宏启用 C++20 模块后，代码将改为导入 `vulkan_hpp`。`stdexcept` 和 `iostream` 头文件用于错误报告，`cstdlib` 提供 `EXIT_SUCCESS` 和 `EXIT_FAILURE` 宏。

程序将所有功能封装在一个类中，Vulkan 对象作为私有成员存储。各初始化函数负责创建相应对象，并由 `initVulkan` 依次调用。准备完成后，程序进入 `mainLoop` 主循环执行渲染。当窗口关闭时，`mainLoop` 返回，随后由 `cleanup` 释放资源。

致命错误会抛出带有描述信息的 `std::runtime_error` 异常，异常会传播至 `main` 函数并打印输出。使用通用的 `std::exception` 捕获以处理各类错误。

每个后续章节都会新增从 `initVulkan` 调用的函数，以及需要在 cleanup 中销毁的 Vulkan 对象。

## 资源管理（Resource Management）

正如 `malloc` 需要对应的 `free`，每一个创建的 Vulkan 对象在不再使用时都需要显式销毁。C++ 支持通过 RAII 或 `<memory>` 中的智能指针实现自动资源管理。本教程展示了使用 RAII 以及现代方法与扩展的 Vulkan 编程实践。

Vulkan 对象通过 `vkCreateXXX` 函数创建，或通过其他对象使用 `vkAllocateXXX` 分配。在确认对象不再被使用后，需分别调用对应的 `vkDestroyXXX` 和 `vkFreeXXX` 进行销毁。不同对象类型的函数参数有所差异，但所有函数都共享一个可选的 `pAllocator` 参数，用于自定义内存分配器回调——本教程中统一传入 `nullptr`。

使用 Vulkan-hpp 的 RAII 模块后，库会自动处理 `vkCreateXXX`、`vkAllocateXXX`、`vkDestroyXXX` 和 `vkFreeXXX` 的调用。

原来的 C 风格代码：

```cpp
VkInstance instance;
VkApplicationInfo appInfo{};
appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
appInfo.pApplicationName = "Hello Triangle";
appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
appInfo.pEngineName = "No Engine";
appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
appInfo.apiVersion = VK_API_VERSION_1_0;

VkInstanceCreateInfo createInfo{};
createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
createInfo.pApplicationInfo = &appInfo;
createInfo.enabledExtensionCount = 0;
createInfo.ppEnabledExtensionNames = nullptr;
createInfo.enabledLayerCount = 0;
createInfo.ppEnabledLayerNames = nullptr;

if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
    throw std::runtime_error("failed to create instance!");
}

vkDestroyInstance(instance, nullptr);
```

现在简化为：

```cpp
constexpr vk::ApplicationInfo appInfo{.pApplicationName   = "Hello Triangle",
                                      .applicationVersion = VK_MAKE_VERSION( 1, 0, 0 ),
                                      .pEngineName        = "No Engine",
                                      .engineVersion      = VK_MAKE_VERSION( 1, 0, 0 ),
                                      .apiVersion         = vk::ApiVersion14};

vk::InstanceCreateInfo createInfo{
    .pApplicationInfo = &appInfo
};

instance = vk::raii::Instance(context, createInfo);
```

## 集成 GLFW（Integrating GLFW）

Vulkan 本身可以独立用于离屏渲染，但将结果显示在屏幕上会更加直观。首先，添加 GLFW。注意 `GLFW_INCLUDE_VULKAN` 宏会让 GLFW 自动包含其自身定义并加载 Vulkan 的 C 头文件。若仅需通过 C 接口获取 Vulkan Surface，使用 `GLFW_INCLUDE_VULKAN` 即可；否则，使用 `GLFW_INCLUDE_NONE` 或不指定同样可以正常工作：

```cpp
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
```

添加一个 `initWindow` 函数，并在 `run` 中的其他调用之前调用它：

```cpp
void run() {
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
}

private:
    void initWindow() {
    }
```

`initWindow` 的第一个调用应为 `glfwInit()`，用于初始化 GLFW 库。由于 GLFW 最初是为 OpenGL 上下文设计的，需要禁止它创建 OpenGL 上下文：

```cpp
glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
```

暂时禁用窗口大小调整，后续章节再处理：

```cpp
glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
```

添加私有成员 `GLFWwindow* window;` 并创建窗口：

```cpp
window = glfwCreateWindow(800, 600, "Vulkan", nullptr, nullptr);
```

前三个参数指定窗口的宽度、高度和标题。第四个参数可选择指定显示器，第五个参数仅与 OpenGL 相关。

使用常量代替硬编码的尺寸，在类定义上方定义：

```cpp
constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;
```

将窗口创建语句替换为：

```cpp
window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
```

完整的 `initWindow` 函数：

```cpp
void initWindow() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
}
```

通过在 `mainLoop` 中添加事件循环，使程序在发生错误或窗口关闭之前持续运行：

```cpp
void mainLoop() {
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }
}
```

该循环会不断检查事件（如用户点击关闭按钮），直到窗口被关闭。后续将在此处调用单帧渲染函数。

窗口关闭后清理资源：

```cpp
void cleanup() {
    glfwDestroyWindow(window);

    glfwTerminate();
}
```

这是本教程中 `cleanup` 函数最终所需的全部内容。

运行程序后，将显示一个标题为"Vulkan"的窗口，直到关闭为止。有了这个框架，下一步就是创建第一个 Vulkan 对象。
