# 物理设备与 Queue Family（Physical Devices and Queue Families）

> 原文：[https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/00_Setup/03_Physical_devices_and_queue_families.html](https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/00_Setup/03_Physical_devices_and_queue_families.html)

## 选择物理设备（Selecting a Physical Device）

通过 `vk::raii::Instance` 初始化 Vulkan 之后，需要在系统中查找并选择一块支持所需功能的显卡。虽然可以同时使用多块显卡，但本教程只选择第一块满足条件的设备。

在初始化序列中添加 `pickPhysicalDevice` 函数：

```cpp
void initVulkan()
{
    createInstance();
    setupDebugMessenger();
    pickPhysicalDevice();
}

void pickPhysicalDevice()
{
}
```

选中的显卡存储为类成员变量：

```cpp
vk::raii::PhysicalDevice physicalDevice = nullptr;
```

设备枚举方式与扩展列举类似：

```cpp
auto physicalDevices = instance.enumeratePhysicalDevices()
```

如果没有找到支持 Vulkan 的设备，则抛出异常：

```cpp
if (physicalDevices.empty())
{
    throw std::runtime_error("failed to find GPUs with Vulkan support!");
}
```

## 基础设备适用性检查（Base Device Suitability Checks）

设备评估通过查询其属性和特性来进行。基础属性（名称、类型、Vulkan 版本）通过以下方式获取：

```cpp
auto deviceProperties = physicalDevice.getProperties();
```

可选特性的支持情况通过以下方式查询：

```cpp
auto deviceFeatures = physicalDevice.getFeatures();
```

可以采用评分机制优先选择独立显卡，并以集成显卡作为备选。本教程实现了四个必要条件：

- 支持 Vulkan 1.3
- 支持 graphics queue family
- 支持所需扩展（`vk::KHRSwapchainExtensionName`）
- 支持所需特性

### API 版本检查（API Version Check）

验证 Vulkan 1.3 兼容性：

```cpp
bool supportsVulkan1_3 = physicalDevice.getProperties().apiVersion >= vk::ApiVersion13;
```

### Queue Family 检查（Queue Family Check）

不同的 queue family 支持不同类型的命令。验证对 graphics 命令的支持：

```cpp
auto queueFamilies = physicalDevice.getQueueFamilyProperties();
bool supportsGraphics =
    std::ranges::any_of(queueFamilies, [](auto const &qfp)
    { return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics); });
```

### 所需扩展检查（Required Extension Check）

验证扩展可用性：

```cpp
std::vector<const char*> requiredDeviceExtension = {vk::KHRSwapchainExtensionName};

auto availableDeviceExtensions = physicalDevice.enumerateDeviceExtensionProperties();
bool supportsAllRequiredExtensions =
  std::ranges::all_of( requiredDeviceExtension,
                       [&availableDeviceExtensions]( auto const & requiredDeviceExtension )
                       {
                           return std::ranges::any_of( availableDeviceExtensions,
                                                       [requiredDeviceExtension]( auto const & availableDeviceExtension )
                                                       { return strcmp( availableDeviceExtension.extensionName, requiredDeviceExtension ) == 0; } );
                       } );
```

### 所需特性检查（Required Feature Check）

验证特性支持：

```cpp
auto features = physicalDevice.template getFeatures2<vk::PhysicalDeviceFeatures2,
    vk::PhysicalDeviceVulkan13Features,
    vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
bool supportsRequiredFeatures =
    features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
    features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;
```

## 完整函数实现（The Complete Function）

完整的设备选择实现综合了所有条件：

```cpp
std::vector<const char*> requiredDeviceExtension = {vk::KHRSwapchainExtensionName};

bool isDeviceSuitable( vk::raii::PhysicalDevice const & physicalDevice )
{
  bool supportsVulkan1_3 = physicalDevice.getProperties().apiVersion >= vk::ApiVersion13;

  auto queueFamilies    = physicalDevice.getQueueFamilyProperties();
  bool supportsGraphics = std::ranges::any_of( queueFamilies, []( auto const & qfp )
      { return !!( qfp.queueFlags & vk::QueueFlagBits::eGraphics ); } );

  auto availableDeviceExtensions = physicalDevice.enumerateDeviceExtensionProperties();
  bool supportsAllRequiredExtensions =
    std::ranges::all_of( requiredDeviceExtension,
                         [&availableDeviceExtensions]( auto const & requiredDeviceExtension )
                         {
                           return std::ranges::any_of( availableDeviceExtensions,
                                                       [requiredDeviceExtension]( auto const & availableDeviceExtension )
                                                       { return strcmp( availableDeviceExtension.extensionName, requiredDeviceExtension ) == 0; } );
                         } );

  auto features =
    physicalDevice
      .template getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features,
      vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
  bool supportsRequiredFeatures =
      features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
      features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

  return supportsVulkan1_3 && supportsGraphics && supportsAllRequiredExtensions && supportsRequiredFeatures;
}

void pickPhysicalDevice()
{
  std::vector<vk::raii::PhysicalDevice> physicalDevices = instance.enumeratePhysicalDevices();
  auto const devIter = std::ranges::find_if( physicalDevices, [&]( auto const & physicalDevice )
      { return isDeviceSuitable( physicalDevice ); } );
  if ( devIter == physicalDevices.end() )
  {
    throw std::runtime_error( "failed to find a suitable GPU!" );
  }
  physicalDevice = *devIter;
}
```

下一步是创建逻辑设备，以与所选的物理设备进行交互。
