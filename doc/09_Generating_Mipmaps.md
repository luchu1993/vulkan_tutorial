# 生成 Mipmap（Generating Mipmaps）

> 原文：https://docs.vulkan.org/tutorial/latest/09_Generating_Mipmaps.html

## 简介（Introduction）

我们的程序现在可以加载并渲染 3D 模型了。在本章中，我们将添加一项功能：mipmap 生成。Mipmap 在游戏和渲染软件中被广泛使用，而 Vulkan 让我们对其创建方式拥有完全的控制权。

Mipmap 是图像预先计算好的、缩小比例的版本。每一张新图像的宽度和高度都是前一张的一半。Mipmap 被用作细节层次（Level of Detail，LOD）的一种形式。距离相机较远的物体将从较小的 mip 图像中采样纹理。使用较小的图像可以提高渲染速度，并避免诸如莫尔纹（Moiré pattern）之类的伪像。

mipmap 示例：

![mipmaps example](https://docs.vulkan.org/tutorial/latest/_images/images/mipmaps_example.jpg)

---

## Image 的创建（Image creation）

在 Vulkan 中，每个 mip 图像存储在 `VkImage` 的不同 mip 层级（mip level）中。Mip level 0 是原始图像，level 0 之后的 mip level 通常被称为 mip chain。Mip level 的数量在创建 `VkImage` 时指定，迄今为止我们始终将其设置为 1。

我们需要根据图像的尺寸计算 mip level 的数量。首先添加一个类成员来存储这个数字：

```cpp
...
uint32_t mipLevels;
std::unique_ptr<vk::raii::Image> textureImage;
...
```

在 `createTextureImage` 中加载纹理后，可以计算出 `mipLevels` 的值：

```cpp
int texWidth, texHeight, texChannels;
stbi_uc* pixels = stbi_load(TEXTURE_PATH.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
...
mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;
```

这段代码计算了 mip chain 中的层级数。`max` 函数选取最大的那个维度。`log2` 函数计算该维度能被 2 整除多少次。`floor` 函数处理最大维度不是 2 的幂次的情况。最后加 1，以便原始图像也占有一个 mip level。

为了使用这个值，我们需要修改 `createImage`、`createImageView` 和 `transitionImageLayout` 函数，以允许我们指定 mip level 的数量。为这些函数添加 `mipLevels` 参数：

```cpp
void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Image& image, vk::raii::DeviceMemory& imageMemory) const {
    ...
    imageInfo.mipLevels = mipLevels;
    ...
}

[[nodiscard]] std::unique_ptr<vk::raii::ImageView> createImageView(const vk::raii::Image& image, vk::Format format, vk::ImageAspectFlags aspectFlags, uint32_t mipLevels) const {
    ...
    viewInfo.subresourceRange.levelCount = mipLevels;
    ...

void transitionImageLayout(const vk::raii::Image& image, const vk::ImageLayout oldLayout, const vk::ImageLayout newLayout, uint32_t mipLevels) const {
    ...
    barrier.subresourceRange.levelCount = mipLevels;
    ...
```

更新所有对这些函数的调用，使用正确的值：

```cpp
createImage(swapChainExtent.width, swapChainExtent.height, 1, depthFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal, depthImage, depthImageMemory);
...
createImage(texWidth, texHeight, mipLevels, vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal, textureImage, textureImageMemory);

swapChainImageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat, vk::ImageAspectFlagBits::eColor, 1);
...
depthImageView = createImageView(depthImage, depthFormat, vk::ImageAspectFlagBits::eDepth, 1);
...
textureImageView = createImageView(textureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor, mipLevels);

transitionImageLayout(depthImage, depthFormat, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal, 1);
...
transitionImageLayout(textureImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, mipLevels);
```

---

## 生成 Mipmap（Generating Mipmaps）

我们的纹理 image 现在有了多个 mip level，但 staging buffer 只能用于填充 mip level 0，其他层级仍未定义。要填充这些层级，我们需要从我们已有的单个层级生成数据。我们将使用 `vkCmdBlitImage` 命令。该命令执行复制、缩放和过滤操作。我们将多次调用它，将数据 blit 到纹理 image 的每个层级。

`vkCmdBlitImage` 被视为 transfer 操作，因此我们必须告知 Vulkan，我们打算将纹理 image 同时用作 transfer 的源和目标。在 `createTextureImage` 中为纹理 image 的 usage flags 添加 `VK_IMAGE_USAGE_TRANSFER_SRC_BIT`：

```cpp
...
createImage(texWidth, texHeight, mipLevels, vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal, textureImage, textureImageMemory);
...
```

与其他 image 操作一样，`vkCmdBlitImage` 依赖于其操作的 image 的 layout。我们可以将整个 image 转换为 `VK_IMAGE_LAYOUT_GENERAL`，但这很可能很慢。为了获得最佳性能，源 image 应处于 `VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL`，目标 image 应处于 `VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL`。

Vulkan 允许我们独立转换 image 的每个 mip level。每次 blit 只处理两个 mip level，因此我们可以在 blit 命令之间将每个 level 转换到最优的 layout。

`transitionImageLayout` 只对整个 image 执行 layout 转换，因此我们需要再写几条 pipeline barrier 命令。在 `createTextureImage` 中删除现有的向 `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL` 的转换：

```cpp
...
transitionImageLayout(textureImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, mipLevels);
    copyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
//transitioned to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL while generating mipmaps
...
```

这将使纹理 image 的每个 level 保持在 `VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL`。每个 level 将在从其读取数据的 blit 命令完成后转换为 `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`。

现在我们来编写生成 mipmap 的函数：

```cpp
void generateMipmaps(vk::raii::Image& image, vk::Format imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels) {
    std::unique_ptr<vk::raii::CommandBuffer> commandBuffer = beginSingleTimeCommands();

    vk::ImageMemoryBarrier barrier (vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferRead
                               , vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal
                               , vk::QueueFamilyIgnored, vk::QueueFamilyIgnored, image);
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    endSingleTimeCommands(commandBuffer);
}
```

我们将进行多次转换，因此我们将复用这个 `VkImageMemoryBarrier`。上面设置的字段对所有 barrier 保持不变。`subresourceRange.miplevel`、`oldLayout`、`newLayout`、`srcAccessMask` 和 `dstAccessMask` 将在每次转换时改变。

```cpp
int32_t mipWidth = texWidth;
int32_t mipHeight = texHeight;

for (uint32_t i = 1; i < mipLevels; i++) {
}
```

这个循环将录制每条 `VkCmdBlitImage` 命令。注意循环变量从 1 开始，而不是 0。

```cpp
barrier.subresourceRange.baseMipLevel = i - 1;
barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

commandBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, barrier);
```

首先，我们将 level `i - 1` 转换为 `VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL`。这个转换将等待 level `i - 1` 被填充——要么来自上一个 blit 命令，要么来自 `vkCmdCopyBufferToImage`。当前的 blit 命令将等待这个转换完成。

```cpp
vk::ArrayWrapper1D<vk::Offset3D, 2> offsets, dstOffsets;
offsets[0] = vk::Offset3D(0, 0, 0);
offsets[1] = vk::Offset3D(mipWidth, mipHeight, 1);
dstOffsets[0] = vk::Offset3D(0, 0, 0);
dstOffsets[1] = vk::Offset3D(mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1);
vk::ImageBlit blit = { .srcSubresource = {}, .srcOffsets = offsets,
                    .dstSubresource =  {}, .dstOffsets = dstOffsets };
blit.srcSubresource = vk::ImageSubresourceLayers( vk::ImageAspectFlagBits::eColor, i - 1, 0, 1);
blit.dstSubresource = vk::ImageSubresourceLayers( vk::ImageAspectFlagBits::eColor, i, 0, 1);
```

接下来，我们指定将在 blit 操作中使用的区域。源 mip level 是 `i - 1`，目标 mip level 是 `i`。`srcOffsets` 数组的两个元素确定数据将从哪个 3D 区域 blit。`dstOffsets` 确定数据将 blit 到哪个区域。`dstOffsets[1]` 的 X 和 Y 维度被除以 2，因为每个 mip level 是前一个 level 大小的一半。`srcOffsets[1]` 和 `dstOffsets[1]` 的 Z 维度必须为 1，因为 2D image 的深度为 1。

```cpp
commandBuffer->blitImage(image, vk::ImageLayout::eTransferSrcOptimal, image, vk::ImageLayout::eTransferDstOptimal, { blit }, vk::Filter::eLinear);
```

现在，我们录制 blit 命令。注意 `textureImage` 同时用作 `srcImage` 和 `dstImage` 参数，这是因为我们在同一个 image 的不同 level 之间进行 blit。源 mip level 刚刚被转换为 `VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL`，目标 level 仍然处于 `createTextureImage` 中的 `VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL`。

注意：如果你使用专用 transfer queue（如 Vertex buffers 章节中建议的），`vkCmdBlitImage` 必须提交到具有 graphics 能力的 queue。

最后一个参数允许我们指定 blit 中使用的 `VkFilter`。我们在这里有与创建 `VkSampler` 时相同的过滤选项。我们使用 `VK_FILTER_LINEAR` 来启用插值。

```cpp
barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

commandBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, barrier);
```

这个 barrier 将 mip level `i - 1` 转换为 `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`。这个转换等待当前 blit 命令完成。所有采样操作都将等待这个转换完成。

```cpp
    ...
    if (mipWidth > 1) mipWidth /= 2;
    if (mipHeight > 1) mipHeight /= 2;
}
```

在循环末尾，我们将当前的 mip 尺寸除以 2。在除法之前检查每个维度，以确保该维度永远不会变为 0。这处理了 image 不是正方形的情况，因为在这种情况下，某一个 mip 维度会在另一个维度之前到达 1。当发生这种情况时，该维度在所有剩余的层级中应该保持为 1。

```cpp
    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

    commandBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, barrier);

    endSingleTimeCommands(*commandBuffer);
}
```

在结束 command buffer 之前，我们再插入一个 pipeline barrier。这个 barrier 将最后一个 mip level 从 `VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL` 转换为 `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`。循环没有处理这个，因为最后一个 mip level 永远不会被 blit。

最后，在 `createTextureImage` 中添加对 `generateMipmaps` 的调用：

```cpp
transitionImageLayout(*textureImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, mipLevels);
copyBufferToImage(stagingBuffer, *textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
//transitioned to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL while generating mipmaps
...
generateMipmaps(textureImage, texWidth, texHeight, mipLevels);
```

我们的纹理 image 的 mipmap 现在已经填充完毕。

---

## 线性过滤支持（Linear filtering support）

使用 `vkCmdBlitImage` 这样的内置函数来生成所有 mip level 非常方便，但不幸的是，并不能保证在所有平台上都受支持。它要求我们使用的纹理 image format 支持线性过滤，这可以通过 `vkGetPhysicalDeviceFormatProperties` 函数来检查。我们将在 `generateMipmaps` 函数中添加相应的检查。

首先，添加一个指定 image format 的参数：

```cpp
void createTextureImage() {
    ...
    generateMipmaps(*textureImage, vk::Format::eR8G8B8A8Srgb, texWidth, texHeight, mipLevels);
}

void generateMipmaps(vk::raii::Image& image, vk::Format imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels) {
    ...
}
```

在 `generateMipmaps` 函数中，使用 `vkGetPhysicalDeviceFormatProperties` 请求纹理 image format 的属性：

```cpp
void generateMipmaps(vk::raii::Image& image, vk::Format imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels) {
    // Check if image format supports linear blit-ing
    vk::FormatProperties formatProperties = physicalDevice->getFormatProperties(imageFormat);
    ...
```

`VkFormatProperties` 结构体有三个字段：`linearTilingFeatures`、`optimalTilingFeatures` 和 `bufferFeatures`，分别描述 format 在不同使用方式下的可用功能。我们使用 optimal tiling format 创建纹理 image，因此需要检查 `optimalTilingFeatures`。对线性过滤特性的支持可以通过 `VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT` 来检查：

```cpp
if (!(formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear)) {
    throw std::runtime_error("texture image format does not support linear blitting!");
}
```

在这种情况下有两种替代方案：你可以实现一个函数，搜索支持线性 blit 的常见纹理 image format；或者你可以使用类似 stb_image_resize 的库在软件中实现 mipmap 生成。然后可以用与加载原始图像相同的方式，将每个 mip level 加载到 image 中。

值得注意的是，在实践中，在运行时生成 mip level 其实并不常见。通常它们是预先生成并与基础层级一起存储在纹理文件中，以提高加载速度。在软件中实现缩放以及从文件加载多个层级的操作留给读者作为练习。

---

## Sampler

虽然 `VkImage` 保存 mipmap 数据，但 `VkSampler` 控制渲染时如何读取这些数据。Vulkan 允许我们指定 `minLod`、`maxLod`、`mipLodBias` 和 `mipmapMode`（"Lod"代表"细节层次"，Level of Detail）。

对纹理进行采样时，sampler 根据以下伪代码选择一个 mip level：

```
lod = getLodLevelFromScreenSize(); // 物体越近值越小，可能为负
lod = clamp(lod + mipLodBias, minLod, maxLod);
level = clamp(floor(lod), 0, texture.mipLevels - 1);  // 限制在纹理的 mip level 数量内
if (mipmapMode == vk::SamplerMipmapMode::eNearest) {
    color = sample(level);
} else {
    color = blend(sample(level), sample(level + 1));
}
```

如果 `samplerInfo.mipmapMode` 是 `VK_SAMPLER_MIPMAP_MODE_NEAREST`，则 `lod` 选择要从中采样的 mip level。如果 mipmap 模式是 `VK_SAMPLER_MIPMAP_MODE_LINEAR`，则 `lod` 用于选择两个要采样的 mip level，对这两个 level 进行采样，并对结果进行线性混合。

采样操作也受 `lod` 影响：

```
if (lod <= 0) {
    color = readTexture(uv, magFilter);
} else {
    color = readTexture(uv, minFilter);
}
```

如果物体靠近相机，则使用 `magFilter` 作为过滤器。如果物体远离相机，则使用 `minFilter`。通常，`lod` 是非负的，只有在靠近相机时才为 0。`mipLodBias` 让我们强制 Vulkan 使用比正常情况下更低的 `lod` 和 `level`。

为了看到本章的结果，我们需要为 `textureSampler` 选择合适的值。我们已经将 `minFilter` 和 `magFilter` 设置为使用 `VK_FILTER_LINEAR`。我们只需要选择 `minLod`、`maxLod`、`mipLodBias` 和 `mipmapMode` 的值。

```cpp
void createTextureSampler() {
    vk::PhysicalDeviceProperties properties = physicalDevice.getProperties();

    vk::SamplerCreateInfo samplerInfo {
        .magFilter = vk::Filter::eLinear,
        .minFilter = vk::Filter::eLinear,
        .mipmapMode = vk::SamplerMipmapMode::eLinear,
        .addressModeU = vk::SamplerAddressMode::eRepeat,
        .addressModeV = vk::SamplerAddressMode::eRepeat,
        .addressModeW = vk::SamplerAddressMode::eRepeat,
        .mipLodBias = 0.0f,
        .anisotropyEnable = vk::True,
        .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
        .compareEnable = vk::False,
        .compareOp = vk::CompareOp::eAlways,
        .minLod = 0.0f,
        .maxLod = vk::LodClampNone
    };
    ...
}
```

在上面的代码中，我们为缩小和放大都设置了线性过滤，并在 mip level 之间进行线性插值。我们还将 mip level 偏置（mip level bias）设置为 `0.0f`。

`minLod` 和 `maxLod` 用于通过限制最小和最大 LOD 值来有效设置使用的 mip level 范围。通过将 `minLod` 设置为 `0.0f`，`maxLod` 设置为 `VK_LOD_CLAMP_NONE`，我们确保将使用全范围的 mip level。

现在运行你的程序，你应该看到以下画面：

![mipmaps](https://docs.vulkan.org/tutorial/latest/_images/images/mipmaps.png)

差别并不明显，因为我们的场景非常简单。如果仔细观察，会发现微妙的差别。最明显的差别是纸张上的文字。

![mipmaps comparison](https://docs.vulkan.org/tutorial/latest/_images/images/mipmaps_comparison.png)

使用 mipmap 后，文字变得平滑。没有 mipmap 时，文字有硬边和因莫尔纹伪像产生的间隙。

你可以调整 sampler 设置来观察它们对 mipmap 的影响。例如，通过改变 `minLod`，你可以强制 sampler 不使用最低的 mip level：

```cpp
samplerInfo.minLod = static_cast<float>(mipLevels / 2);
```

这些设置将产生以下图像：

![high mipmaps](https://docs.vulkan.org/tutorial/latest/_images/images/highmipmaps.png)

这展示了当物体距相机较远时，较高的 mip level 将如何被使用。

下一章将引导我们了解多重采样（multisampling），以产生更平滑的图像。
