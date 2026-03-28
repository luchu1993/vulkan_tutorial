# 图像（Images）

> 原文：[https://docs.vulkan.org/tutorial/latest/06_Texture_mapping/00_Images.html](https://docs.vulkan.org/tutorial/latest/06_Texture_mapping/00_Images.html)

## 简介

到目前为止，我们一直使用逐顶点颜色来为几何体着色，这是一种相当有限的方法。在本章节中，我们将实现 texture mapping，以使几何体看起来更有趣。这也将让我们在未来的章节中加载和绘制基本的三维模型。

向我们的应用程序添加纹理需要以下步骤：

* 创建一个由设备内存支持的 image 对象
* 用图像文件中的像素填充它
* 创建一个 image sampler
* 添加一个 combined image sampler descriptor 以从纹理中采样颜色

我们之前已经使用过 image 对象，但那些是由 swap chain 扩展自动创建的。这次我们需要自己创建一个。创建 image 并填充数据与创建 vertex buffer 类似，但由于 GPU 处理图像的方式不同，会有一些额外的复杂性。我们将先创建一个 staging 资源并用像素数据填充它，然后将其复制到最终的 image 对象中用于渲染。虽然可以为此目的创建一个 staging image，但 Vulkan 也允许你直接从 buffer 将像素复制到 image。这种方式更简洁、限制更少，通常也更快。为此，我们首先创建一个可由主机访问的 buffer，并用像素值填充它，然后再创建一个 image 并将像素复制进去。创建 image 与创建 buffer 并没有太大的不同，同样涉及查询内存需求、分配设备内存并绑定，就像我们之前看到的那样。

然而，在处理 image 时还有一件额外的事情需要注意。Image 可以有不同的 _layout_，这会影响像素在内存中的组织方式。由于图形硬件的工作方式，简单地按行存储像素可能无法获得最佳性能。对 image 执行任何操作时，必须确保它们处于对该操作最优化的 layout。在我们指定 render pass 时，实际上已经见过其中一些 layout：

* `vk::ImageLayout::ePresentSrcKHR`：最适合用于呈现（presentation）
* `vk::ImageLayout::eColorAttachmentOptimal`：最适合作为从 fragment shader 写入颜色的 attachment
* `vk::ImageLayout::eTransferSrcOptimal`：最适合作为传输操作（如 `vkCmdCopyImageToBuffer`）的源
* `vk::ImageLayout::eTransferDstOptimal`：最适合作为传输操作（如 `vkCmdCopyBufferToImage`）的目标
* `vk::ImageLayout::eShaderReadOnlyOptimal`：最适合从 shader 中采样

转换 image layout 最常用的方法之一是使用 _pipeline barrier_。Pipeline barrier 主要用于同步对资源的访问，例如确保在读取之前已完成写入，但也可以用于转换 layout。在本章中，我们将了解 pipeline barrier 是如何用于此目的的。当使用 `vk::SharingMode::eExclusive` 时，barrier 还可以用于在队列族之间转移所有权。

## 加载图像（Loading an image）

像这样引入图像库：

```cpp
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
```

该头文件默认只定义函数的原型。某一个代码文件需要在包含该头文件时加上 `STB_IMAGE_IMPLEMENTATION` 定义以包含函数体，否则会出现链接错误。

```cpp
void initVulkan() {
    ...
    createCommandPool();
    createTextureImage();
    createVertexBuffer();
    ...
}

...

void createTextureImage() {

}
```

创建一个新的函数 `createTextureImage`，我们将在其中加载图像并将其上传到 Vulkan image 对象中。我们将使用 command buffer，所以它应该在 `createCommandPool` 之后调用。

在 `shaders` 目录旁边创建一个新目录 `textures` 来存储纹理图像。我们将从该目录加载一个名为 `texture.jpg` 的图像。我选择使用以下经过 CC0 授权并调整为 512 x 512 像素的图像，但你可以随意选择任何你想要的图像。该库支持最常见的图像文件格式，如 JPEG、PNG、BMP 和 GIF。

使用这个库加载图像非常简单：

```cpp
void createTextureImage() {
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load("textures/texture.jpg", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    vk::DeviceSize imageSize = texWidth * texHeight * 4;

    if (!pixels) {
        throw std::runtime_error("failed to load texture image!");
    }
}
```

`stbi_load` 函数接受文件路径和要加载的通道数作为参数。`STBI_rgb_alpha` 值强制图像以 alpha 通道加载，即使它没有 alpha 通道，这对于将来与其他纹理保持一致性很有用。中间三个参数是图像宽度、高度和实际通道数的输出。返回的指针是像素值数组中的第一个元素。对于 `STBI_rgb_alpha`，像素按行排列，每个像素 4 个字节，共 `texWidth * texHeight * 4` 个值。

## Staging buffer

接下来，我们需要将图像上传到 GPU，以便在 shader 读取时获得最优的访问性能。为此，我们将在主机可见内存中创建一个 buffer，以便将像素复制进去。这个 buffer 将作为数据复制到 GPU 的源。

Image 应该始终驻留在 GPU 内存中。将它们留在仅主机可见的内存中会要求 GPU 每帧通过 PCI 接口读取数据，其带宽比 GPU 自己的内存小得多，这将造成很大的性能影响。Staging 是将图像数据传输到 GPU 内存的过程。它并非总是必要的，因为设备可能提供既主机可见又设备本地的内存类型。在这种配置下，可以跳过 staging。

向 `createTextureImage` 函数中添加此临时 buffer 的变量：

```cpp
vk::raii::Buffer stagingBuffer({});
vk::raii::DeviceMemory stagingBufferMemory({});
```

该 buffer 应位于主机可见内存（`eHostVisible`）中，以便我们可以映射它。它还应该是主机一致的（`eHostCoherent`），以确保写入其中的数据立即可用（而不是以某种方式缓存）。并且它应该可以用作传输源（`eTransferSrc`），以便我们稍后可以将其复制到 image 中：

```cpp
createBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);
```

然后，我们可以直接将从图像加载库获得的像素值复制到 buffer 中：

```cpp
void* data = stagingBufferMemory.mapMemory(0, imageSize);
memcpy(data, pixels, imageSize);
stagingBufferMemory.unmapMemory();
```

记得现在清理原始像素数组：

```cpp
stbi_image_free(pixels);
```

## Texture Image

虽然我们可以设置 shader 来访问 buffer 中的像素值，但在 Vulkan 中使用 image 对象更为合适。Image 对象通过允许我们使用二维坐标，将使检索颜色更容易也更快速。Image 对象中的像素称为 texel，从这一点起我们将使用这个名称。添加以下新的类成员：

```cpp
vk::raii::Image textureImage = nullptr;
vk::raii::DeviceMemory textureImageMemory = nullptr;
```

image 的参数在 `vk::ImageCreateInfo` 结构体中指定：

```cpp
vk::ImageCreateInfo imageInfo{ .imageType = vk::ImageType::e2D, .format = format,
    .extent = {width, height, 1}, .mipLevels = 1, .arrayLayers = 1, .samples = vk::SampleCountFlagBits::e1,
    .tiling = tiling, .usage = usage, .sharingMode = vk::SharingMode::eExclusive};
```

`imageType` 字段中指定的 image 类型告诉 Vulkan image 中的 texel 将用什么样的坐标系来寻址。可以创建 1D、2D 和 3D 的 image。一维 image 可以用来存储数据数组或渐变，二维 image 主要用于纹理，三维 image 可以用来存储体素体积，例如。`extent` 字段指定 image 的维度，基本上是每个轴上有多少个 texel。这就是为什么 `depth` 必须是 `1` 而不是 `0`。我们的纹理不会是数组，暂时也不使用 mipmap。

Vulkan 支持许多可能的 image 格式，但我们应该对 texel 使用与 buffer 中的像素相同的格式，否则复制操作会失败。

`tiling` 字段指定 texel 在内存中的排列方式。Vulkan 为此支持两种根本不同的模式：

* `vk::ImageTiling::eOptimal`：Texel 以依赖于具体实现的方式排列，这会产生更高效的内存访问。
* `vk::ImageTiling::eLinear`：Texel 以行优先的顺序排列在内存中，每行可能有一些填充。

线性排列的 image 非常有限，例如它们可能只适用于二维 image，不能用于深度/模板，不能有多个 mip level 或 layer。GPU 访问速度也比最优排列的 image 慢得多。所以它们的使用场景非常少。

因此，我们将使用 `vk::ImageTiling::eOptimal` 以便从 shader 进行高效访问。

image 的 `initialLayout` 只有两个可能的值：

* `vk::ImageLayout::eUndefined`：GPU 无法使用，第一次转换时会丢弃 texel。
* `vk::ImageLayout::ePreinitialized`：GPU 无法使用，但第一次转换时会保留 texel。

很少有情况下需要在第一次转换期间保留 texel。一个例子是，如果你想使用线性排列的 image 作为 staging image。在这种情况下，你需要向其上传 texel 数据，然后将 image 转换为传输源而不丢失数据。

然而在我们的情况下，我们首先要将 image 转换为传输目标，然后将 texel 数据从 buffer 对象复制到其中，所以我们不需要这个属性，可以安全地使用 `vk::ImageLayout::eUndefined`。

`usage` 字段与 buffer 创建时的语义相同。image 将被用作 buffer 复制的目标，所以它应该被设置为传输目标。我们也希望能够从 shader 中访问 image 来为网格着色，所以 usage 应该包含 `vk::ImageUsageFlagBits::eSampled`。

该 image 只会被一个队列族使用：即支持图形（因此也支持传输）操作的队列族。

`samples` 标志与多重采样有关。这仅与将用作 attachment 的 image 相关，所以坚持使用一个样本。对于 image 还有一些与稀疏 image 相关的可选标志。稀疏 image 是指只有某些区域实际上由内存支持的 image。例如，如果你在一个体素地形中使用 3D 纹理，那么你可以使用这个来避免分配内存来存储大量的"空气"值。在本教程中我们不会使用它，所以将其保留为默认值 `0`。

```cpp
image = vk::raii::Image(device, imageInfo);
```

image 是使用 `vk::raii::Image` 构造函数创建的，没有什么特别值得注意的参数。`vk::Format::eR8G8B8A8Srgb` 格式有可能不被图形硬件支持。你应该有一个可接受的替代格式列表，并选择其中支持的最佳格式。但是，对这种特定格式的支持非常广泛，所以我们跳过这一步。使用不同的格式还需要进行令人烦恼的转换。我们将在深度缓冲章节中回到这一点，届时我们将实现这样一个系统。

```cpp
vk::MemoryRequirements memRequirements = image.getMemoryRequirements();
vk::MemoryAllocateInfo allocInfo{.allocationSize  = memRequirements.size,
                                    .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties)};
imageMemory = vk::raii::DeviceMemory(device, allocInfo);
image.bindMemory(imageMemory, 0);
```

为 image 分配内存的方式与为 buffer 分配内存完全相同。使用默认的 `vk::raii::DeviceMemory` 构造函数，并对 `image` 使用 `bindMemory`。

这个函数已经变得相当大了，在后面的章节中还需要创建更多的 image，所以我们应该将 image 创建抽象为一个 `createImage` 函数，就像我们对 buffer 所做的那样。创建该函数并将 image 对象创建和内存分配移入其中：

```cpp
void createImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Image& image, vk::raii::DeviceMemory& imageMemory) {
    vk::ImageCreateInfo imageInfo{ .imageType = vk::ImageType::e2D, .format = format,
        .extent = {width, height, 1}, .mipLevels = 1, .arrayLayers = 1,
        .samples = vk::SampleCountFlagBits::e1, .tiling = tiling,
        .usage = usage, .sharingMode = vk::SharingMode::eExclusive };

    image = vk::raii::Image(device, imageInfo);

    vk::MemoryRequirements memRequirements = image.getMemoryRequirements();
    vk::MemoryAllocateInfo allocInfo{ .allocationSize = memRequirements.size,
                                        .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties) };
    imageMemory = vk::raii::DeviceMemory(device, allocInfo);
    image.bindMemory(imageMemory, 0);
}
```

我将宽度、高度、格式、tiling 模式、用途和内存属性作为参数，因为这些在本教程中创建的各种 image 之间都会有所不同。

`createTextureImage` 函数现在可以简化为：

```cpp
void createTextureImage() {
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load("textures/texture.jpg", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    vk::DeviceSize imageSize = texWidth * texHeight * 4;

    if (!pixels) {
        throw std::runtime_error("failed to load texture image!");
    }

    vk::raii::Buffer stagingBuffer({});
    vk::raii::DeviceMemory stagingBufferMemory({});
    createBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

    void* data = stagingBufferMemory.mapMemory(0, imageSize);
    memcpy(data, pixels, imageSize);
    stagingBufferMemory.unmapMemory();

    stbi_image_free(pixels);

    vk::raii::Image textureImageTemp({});
    vk::raii::DeviceMemory textureImageMemoryTemp({});
    createImage(texWidth, texHeight, vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal, textureImageTemp, textureImageMemoryTemp);
}
```

## Layout 转换（Layout transitions）

如前所述，Vulkan 中的 image 可以存在于不同的 layout 中，这些 layout 会影响像素数据在内存中的组织方式。这些 layout 针对特定操作进行了优化——有些 layout 更适合从 shader 中读取，有些更适合作为渲染目标，还有一些更适合作为传输操作的源或目标。

Layout 转换是 Vulkan 设计中的一个关键方面，它给你对这些内存组织方式的显式控制。与某些其他图形 API 中驱动程序自动处理这些转换不同，Vulkan 要求你明确地管理它们。这种方法允许更好的性能优化，因为你可以在需要时精确地安排转换并高效地批量处理操作。

对于我们的 texture image，我们需要执行几次转换：
1. 从初始的 undefined layout 转换到适合接收数据的 layout（传输目标）
2. 从传输目标转换到适合 shader 读取的 layout，以便我们的 fragment shader 可以从中采样

这些转换使用 pipeline barrier 来执行，pipeline barrier 不仅会更改 image layout，还确保访问 image 的操作之间的正确同步。如果没有适当的同步，我们可能会遇到竞态条件，即 shader 在复制操作完成之前尝试从纹理中读取。

我们现在要编写的函数再次涉及记录和执行 command buffer，所以现在是将该逻辑移入一两个辅助函数的好时机：

```cpp
vk::raii::CommandBuffer beginSingleTimeCommands() {
    vk::CommandBufferAllocateInfo allocInfo{ .commandPool = commandPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1 };
    vk::raii::CommandBuffer commandBuffer = std::move(device.allocateCommandBuffers(allocInfo).front());

    vk::CommandBufferBeginInfo beginInfo{ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit };
    commandBuffer.begin(beginInfo);

    return commandBuffer;
}

void endSingleTimeCommands(vk::raii::CommandBuffer& commandBuffer) {
    commandBuffer.end();

    vk::SubmitInfo submitInfo{ .commandBufferCount = 1, .pCommandBuffers = &*commandBuffer };
    graphicsQueue.submit(submitInfo, nullptr);
    graphicsQueue.waitIdle();
}
```

这些函数的代码基于 `copyBuffer` 中的现有代码。现在可以将该函数简化为：

```cpp
void copyBuffer(vk::raii::Buffer & srcBuffer, vk::raii::Buffer & dstBuffer, vk::DeviceSize size) {
    vk::raii::CommandBuffer commandCopyBuffer = beginSingleTimeCommands();
    commandCopyBuffer.copyBuffer(srcBuffer, dstBuffer, vk::BufferCopy(0, 0, size));
    endSingleTimeCommands(commandCopyBuffer);
}
```

如果我们还在使用 buffer，那么我们现在可以编写一个函数来记录和执行 `copyBufferToImage` 来完成工作，但这个命令要求 image 首先处于正确的 layout。创建一个新函数来处理 layout 转换：

```cpp
void transitionImageLayout(const vk::raii::Image& image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout) {
    auto commandBuffer = beginSingleTimeCommands();

    endSingleTimeCommands(commandBuffer);
}
```

执行 layout 转换最常见的方法之一是使用 _image memory barrier_。这样的 pipeline barrier 通常用于同步对资源的访问，例如确保在读取之前完成对 buffer 的写入，但也可以用于转换 image layout 以及在使用 `vk::SharingMode::eExclusive` 时转移队列族所有权。对于 buffer 也有等效的 _buffer memory barrier_。

```cpp
vk::ImageMemoryBarrier barrier{ .oldLayout = oldLayout, .newLayout = newLayout, .image = image, .subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 } };
```

`oldLayout` 和 `newLayout` 指定 layout 转换。如果你不在乎 image 现有的内容，可以使用 `vk::ImageLayout::eUndefined` 作为 `oldLayout`。

如果你使用 barrier 来转移队列族所有权，那么这两个字段应该是队列族的索引。如果你不想这样做，必须将它们设置为 `VK_QUEUE_FAMILY_IGNORED`（不是默认值！）。

`image` 和 `subresourceRange` 指定受影响的 image 以及 image 的特定部分。我们的 image 不是数组，也没有 mipmap level，所以只指定一个 level 和 layer。

Barrier 主要用于同步目的，所以你必须指定哪些涉及资源的操作类型必须发生在 barrier 之前，以及哪些涉及资源的操作必须等待 barrier。尽管我们已经使用 `queue.waitIdle()` 手动同步，我们仍然需要这样做。正确的值取决于新旧 layout，所以一旦我们确定了将要使用的转换，我们会回到这里处理。

```cpp
commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, {}, nullptr, barrier);
```

所有类型的 pipeline barrier 都使用同一个函数提交。command buffer 之后的第一个参数指定在 barrier 之前应该发生的操作所在的 pipeline 阶段。第二个参数指定操作将在哪个 pipeline 阶段等待 barrier。你被允许在 barrier 之前和之后指定的 pipeline 阶段取决于你在 barrier 之前和之后如何使用资源。允许的值在规范的该表中列出。例如，如果你打算在 barrier 之后从 uniform 中读取，你会指定 `vk::AccessFlagBits::eUniformRead` 的用途，以及将从 uniform 中读取的最早 shader 作为 pipeline 阶段，例如 `vk::PipelineStageFlagBits::eFragmentShader`。对于这种类型的用途指定非 shader pipeline 阶段没有意义，验证层会在你指定与用途类型不匹配的 pipeline 阶段时发出警告。

第三个参数是 `0` 或 `vk::DependencyFlagBits::eByRegion`。后者将 barrier 变成每区域的条件。这意味着例如，实现被允许已经从资源中已写入的部分开始读取。

最后三对参数引用三种可用类型的 pipeline barrier 数组：memory barrier、buffer memory barrier 和 image memory barrier（如我们这里使用的）。注意，我们还没有使用 `VkFormat` 参数，但我们将在深度缓冲章节的特殊转换中使用它。

## 将 buffer 复制到 image（Copying buffer to image）

在我们回到 `createTextureImage` 之前，我们还要编写一个辅助函数：`copyBufferToImage`：

```cpp
void copyBufferToImage(const vk::raii::Buffer& buffer, vk::raii::Image& image, uint32_t width, uint32_t height) {
    vk::raii::CommandBuffer commandBuffer = beginSingleTimeCommands();

    endSingleTimeCommands(commandBuffer);
}
```

与 buffer 复制一样，你需要指定 buffer 的哪一部分将被复制到 image 的哪一部分。这通过 `vk::BufferImageCopy` 结构体来实现：

```cpp
vk::BufferImageCopy region{ .bufferOffset = 0, .bufferRowLength = 0, .bufferImageHeight = 0,
    .imageSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 }, .imageOffset = {0, 0, 0}, .imageExtent = {width, height, 1} };
```

大多数字段都是不言自明的。`bufferOffset` 指定 buffer 中像素值开始的字节偏移量。`bufferRowLength` 和 `bufferImageHeight` 字段指定像素在内存中的排列方式。例如，image 的行之间可能有一些填充字节。两者都指定为 `0` 表示像素像我们这里一样紧密排列。`imageSubresource`、`imageOffset` 和 `imageExtent` 字段指示我们要将像素复制到 image 的哪个部分。

Buffer 到 image 的复制操作使用 `vkCmdCopyBufferToImage` 函数排队：

```cpp
commandBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, {region});
// Submit the buffer copy to the graphics queue
endSingleTimeCommands(*commandBuffer);
```

第四个参数指示 image 当前使用的 layout。这里我假设 image 已经被转换为最适合将像素复制进去的 layout。目前我们只将一块像素复制到整个 image，但可以指定 `vk::BufferImageCopy` 的数组，以便在一次操作中从这个 buffer 执行许多不同的复制到 image。

## 准备 texture image（Preparing the texture image）

现在我们拥有了完成 texture image 设置所需的所有工具，所以我们回到 `createTextureImage` 函数。我们在那里做的最后一件事是创建 texture image。下一步是将 staging buffer 复制到 texture image。这涉及两个步骤：

* 将 texture image 转换为 `vk::ImageLayout::eTransferDstOptimal`
* 执行 buffer 到 image 的复制操作

使用我们刚刚创建的函数很容易做到这一点：

```cpp
transitionImageLayout(textureImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
copyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
```

image 是以 `vk::ImageLayout::eUndefined` layout 创建的，所以在转换 `textureImage` 时应该将其指定为旧 layout。请记住，我们可以这样做是因为在执行复制操作之前我们不关心其内容。

为了能够在 shader 中从 texture image 开始采样，我们还需要最后一次转换为 shader 访问做好准备：

```cpp
transitionImageLayout(textureImage, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
```

## 转换 barrier 的 mask（Transition barrier masks）

如果你现在启用验证层运行应用程序，你会看到它抱怨 `transitionImageLayout` 中的 access mask 和 pipeline 阶段无效。我们仍然需要根据转换中的 layout 来设置这些。

我们需要处理两种转换：

* Undefined → 传输目标：传输写入不需要等待任何事情
* 传输目标 → shader 读取：shader 读取应该等待传输写入，特别是 fragment shader 中的 shader 读取，因为那是我们将要使用纹理的地方

这些规则使用以下 access mask 和 pipeline 阶段来指定：

```cpp
vk::PipelineStageFlags sourceStage;
vk::PipelineStageFlags destinationStage;

if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal) {
    barrier.srcAccessMask = {};
    barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

    sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
    destinationStage = vk::PipelineStageFlagBits::eTransfer;
} else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
    barrier.srcAccessMask =  vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask =  vk::AccessFlagBits::eShaderRead;

    sourceStage = vk::PipelineStageFlagBits::eTransfer;
    destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
} else {
    throw std::invalid_argument("unsupported layout transition!");
}

commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, {}, nullptr, barrier);
endSingleTimeCommands(*commandBuffer);
```

如你在前述表格中所见，传输写入必须发生在 pipeline 传输阶段。由于写入不必等待任何事情，你可以为 barrier 前的操作指定空的 access mask 和最早可能的 pipeline 阶段 `vk::PipelineStageFlagBits::eTopOfPipe`。需要注意的是，`vk::PipelineStageFlagBits::eTransfer` 并不是图形和计算 pipeline 中的一个 _真实_ 阶段，它更像是一个传输发生的伪阶段。有关伪阶段的更多信息和其他示例，请参阅文档。

Image 将在同一个 pipeline 阶段写入，随后由 fragment shader 读取，这就是为什么我们在 fragment shader pipeline 阶段指定 shader 读取访问。

如果我们将来需要进行更多的转换，我们将扩展该函数。应用程序现在应该可以成功运行，尽管当然还没有视觉上的变化。

需要注意的一点是，command buffer 提交会在开始时隐式产生 `vk::AccessFlagBits::eHostWrite` 同步。由于 `transitionImageLayout` 函数只执行一个具有单个命令的 command buffer，如果你在 layout 转换中需要 `vk::AccessFlagBits::eHostWrite` 依赖，可以使用这种隐式同步并将 `srcAccessMask` 设置为 `0`。是否要对此明确，由你自己决定，但我个人不喜欢依赖这些类似 OpenGL 的"隐藏"操作。

实际上有一种特殊的 image layout 支持所有操作，`vk::ImageLayout::eGeneral`。但是除非使用某些扩展（在本教程中我们不使用），使用通用 layout 可能会带来性能损失，因为它可能会在某些 GPU 上禁用某些优化。在某些特殊情况下需要使用它，例如将 image 同时用作输入和输出，或者在 image 离开预初始化 layout 后读取它。

到目前为止，所有提交命令的辅助函数都设置为通过等待队列变为空闲来同步执行。对于实际应用程序，建议将这些操作组合在单个 command buffer 中并异步执行以获得更高的吞吐量，特别是 `createTextureImage` 函数中的转换和复制。可以通过创建一个辅助函数记录命令的 `setupCommandBuffer`，以及添加一个执行到目前为止记录的命令的 `flushSetupCommands` 来进行实验。最好在 texture mapping 正常工作后再这样做，以检查纹理资源是否仍然正确设置。

Image 现在包含了纹理，但我们仍然需要一种从图形 pipeline 中访问它的方法。我们将在下一章节中处理这个问题。
