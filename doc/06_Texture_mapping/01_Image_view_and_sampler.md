# 图像视图与采样器（Image view and sampler）

> 原文：[https://docs.vulkan.org/tutorial/latest/06_Texture_mapping/01_Image_view_and_sampler.html](https://docs.vulkan.org/tutorial/latest/06_Texture_mapping/01_Image_view_and_sampler.html)

## Texture image view

在本章中，我们将为之前章节中创建的纹理创建两个额外的资源。第一个资源是我们之前在处理 swap chain image 时已经见过的，但第二个是新的——它涉及 shader 如何从图像中读取 texel。

与 swap chain image 一样，texture image 也需要通过 image view 来访问，而不是直接访问。为此，我们需要添加一个类成员来存储 texture image view，并创建一个新的函数 `createTextureImageView`：

```cpp
vk::raii::ImageView textureImageView = nullptr;

void initVulkan() {
    ...
    createTextureImage();
    createTextureImageView();
    createVertexBuffer();
    ...
}

void createTextureImageView() {

}
```

该函数的代码可以直接基于 `createImageViews`，只有两处不同：格式（format）和图像（image）：

```cpp
vk::ImageViewCreateInfo viewInfo{ .image = image, .viewType = vk::ImageViewType::e2D, .format = format, .subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }};
```

我省略了显式的 `vk::ComponentSwizzle::eIdentity` 值，因为 Vulkan 允许使用全零来表示 identity swizzle。通过以下调用完成 image view 的创建：

```cpp
vk::raii::ImageView( device, viewInfo );
```

由于 `createTextureImageView` 和 `createImageViews` 的逻辑非常相似，你可以将其抽象为一个辅助函数 `createImageView`：

```cpp
vk::raii::ImageView createImageView(vk::raii::Image& image, vk::Format format) {
    vk::ImageViewCreateInfo viewInfo{ .image = image, .viewType = vk::ImageViewType::e2D,
        .format = format, .subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 } };
    return vk::raii::ImageView( device, viewInfo );
}
```

`createTextureImageView` 函数现在可以简化为：

```cpp
void createTextureImageView() {
    textureImageView = createImageView(textureImage, vk::Format::eR8G8B8A8Srgb);
}
```

`createImageViews` 也可以简化为：

```cpp
void createImageViews() {
    swapChainImageViews.resize(swapChainImages.size());

    for (uint32_t i = 0; i < swapChainImages.size(); i++) {
        swapChainImageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat);
    }
}
```

## 采样器（Samplers）

Shader 可以直接从 image 中读取 texel，但当 image 被用作纹理时，这通常不是最佳做法。纹理通常通过 sampler 来访问，sampler 会自动应用过滤（filtering）和变换（transformation）来计算最终颜色。

这些过滤器对于处理诸如 **过采样（oversampling）** 这类问题很有帮助。考虑一个纹理被映射到片元数量多于 texel 数量的几何体上的情况。如果你简单地为每个片元取最近的 texel，你会得到类似第一张图的结果：

![过采样示例](https://docs.vulkan.org/tutorial/latest/_images/images/texture_filtering.png)

如果你通过线性插值结合 4 个最近的 texel，你会得到更平滑的结果，如右图所示。当然，你的应用程序可能有艺术风格的需求，更适合左图那种风格（想想 Minecraft），但在常规的图形应用程序中，右图通常是首选。sampler 对象在你从 shader 中读取颜色时会自动应用这种过滤。

**欠采样（undersampling）** 是相反的问题，即 texel 数量多于片元数量。这会在以锐角采样高频图案（如棋盘格纹理）时产生伪影：

![欠采样示例](https://docs.vulkan.org/tutorial/latest/_images/images/anisotropic_filtering.png)

如左图所示，纹理在远处变得模糊混乱。解决方法是各向异性过滤（anisotropic filtering），它也可以由 sampler 自动应用。

除了这些过滤器，sampler 还可以处理 **寻址模式（addressing mode）**。下图展示了可能的寻址模式之一：

![寻址模式示例](https://docs.vulkan.org/tutorial/latest/_images/images/texture_addressing.png)

现在我们将创建一个函数 `createTextureSampler` 来设置这样一个 sampler 对象。稍后我们将使用它在 shader 中从纹理中读取颜色。

```cpp
void initVulkan() {
    ...
    createTextureImage();
    createTextureImageView();
    createTextureSampler();
    ...
}

void createTextureSampler() {

}
```

Sampler 通过 `vk::SamplerCreateInfo` 结构体来配置，该结构体指定了它应该应用的所有过滤器和变换。

```cpp
vk::SamplerCreateInfo samplerInfo{.magFilter = vk::Filter::eLinear, .minFilter = vk::Filter::eLinear};
```

`magFilter` 和 `minFilter` 字段指定如何对放大（magnified）或缩小（minified）的 texel 进行插值。放大涉及上述的过采样问题，缩小涉及欠采样问题。可选项有 `vk::Filter::eNearest` 和 `vk::Filter::eLinear`，分别对应前面图中演示的两种模式。

```cpp
vk::SamplerCreateInfo samplerInfo{.magFilter = vk::Filter::eLinear, .minFilter = vk::Filter::eLinear,  .mipmapMode = vk::SamplerMipmapMode::eLinear,
    .addressModeU = vk::SamplerAddressMode::eRepeat, .addressModeV = vk::SamplerAddressMode::eRepeat };
```

寻址模式可以通过 `addressModeU`、`addressModeV` 和 `addressModeW` 字段按轴分别指定。可用的值列在下面。上图中展示了其中的大部分。

* `vk::SamplerAddressMode::eRepeat`：当超出图像维度时重复纹理。
* `vk::SamplerAddressMode::eMirroredRepeat`：与 repeat 类似，但在超出维度时镜像图像以获得反转的效果。
* `vk::SamplerAddressMode::eClampToEdge`：取图像维度之外最接近坐标的边缘颜色。
* `vk::SamplerAddressMode::eMirrorClampToEdge`：与 clamp to edge 类似，但使用与最近边缘相对的边缘。
* `vk::SamplerAddressMode::eClampToBorder`：当采样超出图像维度时返回纯色。

在本教程中我们使用哪种寻址模式并不重要，因为我们不会采样图像之外的区域。然而，repeat 模式可能是最常用的，因为它可以用来将地板和墙壁等纹理铺贴开来。

```cpp
vk::PhysicalDeviceProperties properties = physicalDevice.getProperties();
vk::SamplerCreateInfo samplerInfo{.magFilter = vk::Filter::eLinear, .minFilter = vk::Filter::eLinear,  .mipmapMode = vk::SamplerMipmapMode::eLinear,
    .addressModeU = vk::SamplerAddressMode::eRepeat, .addressModeV = vk::SamplerAddressMode::eRepeat, .addressModeW = vk::SamplerAddressMode::eRepeat,
    .anisotropyEnable = vk::True, .maxAnisotropy = properties.limits.maxSamplerAnisotropy};
```

这两个字段指定是否应该使用各向异性过滤。除非性能是一个问题，否则没有理由不使用它。`maxAnisotropy` 字段限制了可以用来计算最终颜色的 texel 采样数量。值越低，性能越好，但质量越差。要弄清楚我们可以使用的最大值，我们需要获取物理设备的属性，如上所示。

要在应用程序中使用 `physicalDevice` 的属性，我们需要获取它们，如上面的代码所示，将其分配给 `properties` 变量。

```cpp
samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
```

`borderColor` 字段指定当以 clamp-to-border 寻址模式采样超出图像范围时返回哪种颜色。可以返回黑色、白色或透明，无论 float 格式或 int 格式。你不能指定任意颜色。

```cpp
samplerInfo.unnormalizedCoordinates = vk::False;
```

`unnormalizedCoordinates` 字段指定你希望用来寻址图像中 texel 的坐标系。如果该字段为 `vk::True`，则可以简单地使用 `[0, texWidth)` 和 `[0, texHeight)` 范围内的坐标。如果为 `vk::False`，则 texel 使用所有轴上的 `[0, 1)` 范围来寻址。实际应用程序几乎总是使用归一化坐标，因为这样就可以用具有完全相同坐标的不同分辨率的纹理。

```cpp
samplerInfo.compareEnable = vk::False;
samplerInfo.compareOp = vk::CompareOp::eAlways;
```

如果启用了比较功能，则 texel 将首先与一个值进行比较，比较结果将用于过滤操作。这主要用于阴影贴图（shadow map）上的 percentage-closer filtering。我们将在以后的章节中介绍这一点。

```cpp
samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
samplerInfo.mipLodBias = 0.0f;
samplerInfo.minLod = 0.0f;
samplerInfo.maxLod = 0.0f;
```

所有这些字段都适用于 mipmap。我们将在后面的章节中介绍 mipmap，但基本上它是另一种可以应用的过滤器。

sampler 的功能现在已经完全定义好了。添加一个类成员来保存 sampler 对象的句柄，并使用 `vk::raii::Sampler` 来创建 sampler：

```cpp
vk::raii::ImageView textureImageView = nullptr;
vk::raii::Sampler textureSampler = nullptr;

void createTextureSampler() {
    ...
    textureSampler = vk::raii::Sampler(device, samplerInfo);
}
```

注意，sampler 没有在任何地方引用 `vk::raii::Image`。sampler 是一个独特的对象，它提供了一个从纹理中提取颜色的接口。它可以应用于任何你想要的图像，无论是 1D、2D 还是 3D。这与许多旧的 API 不同，旧的 API 将纹理图像和过滤组合到一个单一的状态中。

## 各向异性过滤设备特性（Anisotropy device feature）

如果你现在运行应用程序，你会看到类似这样的验证层消息：

> vkCreateSampler(): pCreateInfo->anisotropyEnable is VK_TRUE but the samplerAnisotropy feature was not enabled.

这是因为各向异性过滤实际上是一个可选的设备特性。我们需要更新 `createLogicalDevice` 函数来请求它：

```cpp
vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT> featureChain = {
    {.features = {.samplerAnisotropy = true } },            // vk::PhysicalDeviceFeatures2
    {.synchronization2 = true, .dynamicRendering = true },  // vk::PhysicalDeviceVulkan13Features
    {.extendedDynamicState = true }                         // vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
};
```

即使现代显卡不太可能不支持它，我们也应该更新 `isDeviceSuitable` 来检查它是否可用：

```cpp
bool isDeviceSuitable(VkPhysicalDevice device) {
    ...

    vk::PhysicalDeviceFeatures supportedFeatures = device.getPhysicalDeviceFeatures();

    return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
}
```

`vkGetPhysicalDeviceFeatures` 函数会重新利用 `vk::PhysicalDeviceFeatures` 结构体来指示哪些特性被支持，而不是被请求，具体方式是将布尔值设置为 `vk::True`。

作为使用各向异性过滤的替代方案，也可以通过条件设置来不使用它：

```cpp
samplerInfo.anisotropyEnable = VK_FALSE;
samplerInfo.maxAnisotropy = 1.0f;
```

在下一章中，我们将把图像和 sampler 对象暴露给 shader，以将纹理绘制到正方形上。
