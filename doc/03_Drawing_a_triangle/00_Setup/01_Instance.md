# 实例（Instance）

> 原文：[https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/00_Setup/01_Instance.html](https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/00_Setup/01_Instance.html)

## 创建实例（Creating an Instance）

初始化 Vulkan 库的第一步是创建一个 **instance**。instance 代表应用程序与 Vulkan 库之间的连接，创建过程中需要向驱动程序提供应用程序的相关信息。

### 实现步骤

**1. 添加 `createInstance` 函数**，并在 `initVulkan()` 中调用：

```cpp
void initVulkan() {
    createInstance();
}
```

**2. 添加成员变量**用于存储 instance 句柄和 RAII context：

```cpp
private:
vk::raii::Context  context;
vk::raii::Instance instance = nullptr;
```

**3. 填充 `ApplicationInfo` 结构体**，包含应用程序元数据：

```cpp
constexpr vk::ApplicationInfo appInfo{
    .pApplicationName   = "Hello Triangle",
    .applicationVersion = VK_MAKE_VERSION( 1, 0, 0 ),
    .pEngineName        = "No Engine",
    .engineVersion      = VK_MAKE_VERSION( 1, 0, 0 ),
    .apiVersion         = vk::ApiVersion14
};
```

**4. 创建 `InstanceCreateInfo` 结构体**，指定扩展和层：

```cpp
vk::InstanceCreateInfo createInfo{
    .pApplicationInfo = &appInfo
};
```

**5. 获取并验证 GLFW 扩展**：

```cpp
uint32_t glfwExtensionCount = 0;
auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

auto extensionProperties = context.enumerateInstanceExtensionProperties();
for (uint32_t i = 0; i < glfwExtensionCount; ++i) {
    if (std::ranges::none_of(extensionProperties,
        [glfwExtension = glfwExtensions[i]](auto const& extensionProperty)
        { return strcmp(extensionProperty.extensionName, glfwExtension) == 0; }))
    {
        throw std::runtime_error("Required GLFW extension not supported: " +
            std::string(glfwExtensions[i]));
    }
}

vk::InstanceCreateInfo createInfo{
    .pApplicationInfo = &appInfo,
    .enabledExtensionCount = glfwExtensionCount,
    .ppEnabledExtensionNames = glfwExtensions
};
```

**6. 创建 instance**：

```cpp
instance = vk::raii::Instance(context, createInfo);
```

### 对象创建模式（Object Creation Pattern）

Vulkan 对象创建遵循一致的模式：
- 指向创建信息结构体的指针
- 指向自定义分配器回调的指针（可选）
- 指向父设备 / instance / context 的指针
- 返回 RAII 构造的对象

### 错误处理（Error Handling）

教程展示了两种处理方式：

**使用 C++ 异常**（推荐）：

```cpp
try {
    vk::raii::Context context;
    vk::raii::Instance instance(context, vk::InstanceCreateInfo{});
    // 使用对象
}
catch (const vk::SystemError& err) {
    std::cerr << "Vulkan error: " << err.what() << std::endl;
    return 1;
}
```

**使用 Result 返回值**（替代方式）：

```cpp
auto instanceRV = context.createInstance(...);
if (!instanceRV.has_value()) {
    std::cerr << "Error: " << vk::to_string(instanceRV.result) << std::endl;
    std::exit(EXIT_FAILURE);
}
```

## macOS 兼容性问题（macOS Compatibility Issue）

对于使用较新版本 MoltenVK SDK 的 macOS 用户，可能会遇到 `vk::Result::eErrorIncompatibleDriver` 错误，此时需要启用 `VK_KHR_PORTABILITY_SUBSET` 扩展：

```cpp
vk::InstanceCreateInfo createInfo{
    .flags = vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR,
    .pApplicationInfo = &appInfo,
    .ppEnabledExtensionNames = { vk::KHRPortabilityEnumerationExtensionName }
};
```

## 检查扩展支持（Checking for Extension Support）

在创建 instance 之前，可以枚举所有可用的扩展：

```cpp
auto extensions = context.enumerateInstanceExtensionProperties();

std::cout << "available extensions:\n";
for (const auto& extension : extensions) {
    std::cout << '\t' << extension.extensionName << '\n';
}
```

每个 `vk::ExtensionProperties` 结构体包含扩展的名称和版本号。
