# 深度缓冲（Depth Buffering）

> 原文：https://docs.vulkan.org/tutorial/latest/07_Depth_buffering.html

## 简介（Introduction）

我们迄今为止处理的几何体被投影到 3D 空间，但它仍然完全是平的。在本章中，我们将为位置添加 Z 坐标，为处理 3D 网格做好准备。我们将使用这个第三坐标在当前正方形上方放置另一个正方形，以展示当几何体未按深度排序时会出现的问题。

---

## 3D 几何体（3D geometry）

修改 `Vertex` 结构体，使位置使用 3D 向量，并更新对应 `vk::VertexInputAttributeDescription` 中的 format：

```cpp
struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;
    ...
    static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions() {
        return {
            vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos)),
            vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color)),
            vk::VertexInputAttributeDescription(2, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, texCoord))
        };
        ...
    }
};
```

接下来，更新 vertex shader，将 `inPosition` 的类型从 `float2` 改为 `float3`，以接受并变换 3D 坐标输入：

```slang
struct VSInput {
    float3 inPosition;
    ...
};
...
[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.pos = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(input.inPosition, 1.0))));
    output.fragColor = input.inColor;
    output.fragTexCoord = input.inTexCoord;
    return output;
}
```

记得事后重新编译 shader！

最后，更新 `vertices` 容器以包含 Z 坐标：

```cpp
const std::vector<Vertex> vertices = {
    {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
};
```

如果你现在运行应用程序，应该看到与之前完全相同的结果。现在是时候添加一些额外的几何体，使场景更加有趣，并展示本章要解决的问题。

复制顶点，在当前正方形正下方定义一个正方形的位置，如下所示：

使用 Z 坐标 `-0.5f` 并为额外的正方形添加适当的索引：

```cpp
const std::vector<Vertex> vertices = {
    {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
    {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
};

const std::vector<uint16_t> indices = {
    0, 1, 2, 2, 3, 0,
    4, 5, 6, 6, 7, 4
};
```

现在运行你的程序，你会看到一些类似 Escher 版画的东西：

![extra square](https://docs.vulkan.org/tutorial/latest/_images/images/extra_square.svg)

问题在于下方正方形的片元（fragment）覆盖了上方正方形的片元，仅仅是因为它在索引数组中出现得更晚。有两种方法可以解决这个问题：

- 将所有绘制调用按深度从后到前排序
- 使用 depth buffer 进行深度测试

第一种方法通常用于绘制透明物体，因为与顺序无关的透明度（order-independent transparency）是一个很难解决的挑战。然而，按深度排序片元的问题更常见的解决方式是使用 depth buffer。

depth buffer 是一个额外的 attachment，用于存储每个位置的深度，就像 color attachment 存储每个位置的颜色一样。每次 rasterizer 产生一个片元时，深度测试都会检查新片元是否比之前的片元更近。如果不是，则新片元被丢弃。通过深度测试的片元将自己的深度写入 depth buffer。你也可以像操纵颜色输出一样，从 fragment shader 中操纵这个值。

```cpp
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
```

GLM 生成的透视投影矩阵默认使用 OpenGL 的深度范围 -1.0 到 1.0。我们需要使用 `GLM_FORCE_DEPTH_ZERO_TO_ONE` 定义将其配置为使用 Vulkan 的深度范围 0.0 到 1.0。

---

## Depth Image 与 Image View（Depth image and view）

depth attachment 基于 image，就像 color attachment 一样。区别在于 swap chain 不会自动为我们创建 depth image。我们只需要一个 depth image，因为同一时刻只有一个绘制操作在运行。depth image 同样需要三件套资源：image、memory 和 image view。

```cpp
vk::raii::Image depthImage = nullptr;
vk::raii::DeviceMemory depthImageMemory = nullptr;
vk::raii::ImageView depthImageView = nullptr;
```

创建一个新函数 `createDepthResources` 来设置这些资源：

```cpp
void initVulkan() {
    ...
    createCommandPool();
    createDepthResources();
    createTextureImage();
    ...
}
...
void createDepthResources() {
}
```

创建 depth image 相当简单。它应该与 color attachment 具有相同的分辨率（由 swap chain extent 定义）、适合用作 depth attachment 的 image usage、optimal tiling 和 device local memory。

唯一的问题是：depth image 的正确 format 是什么？

format 必须包含深度分量，在 `vk::Format` 中以 `D??` 表示。与 texture image 不同，我们不一定需要特定的 format，因为我们不会直接从程序中访问 texel。它只需要有合理的精度，在实际应用中至少 24 位是常见要求。有几种 format 满足此要求：

- `vk::Format::eD32Sfloat`：32 位浮点深度
- `vk::Format::eD32SfloatS8Uint`：32 位有符号浮点深度 + 8 位 stencil 分量
- `vk::Format::eD24UnormS8Uint`：24 位浮点深度 + 8 位 stencil 分量

stencil 分量用于模板测试（stencil test），这是一种可以与深度测试结合使用的额外测试。我们将在未来的章节中了解这一点。

我们可以直接选择 `vk::Format::eD32Sfloat` format，因为它的支持极为普遍（参见硬件数据库），但在可能的情况下为应用程序添加一些额外的灵活性也是不错的。我们将编写一个 `findSupportedFormat` 函数，该函数按从最优先到最不优先的顺序接受候选 format 列表，并检查哪一个是第一个受支持的：

```cpp
vk::Format findSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features) {
}
```

format 的支持取决于 tiling 模式和用途，因此我们还必须将这些作为参数。format 的支持可以使用 `physicalDevice.getFormatProperties` 函数查询：

```cpp
for (const auto format : candidates) {
    vk::FormatProperties props = physicalDevice.getFormatProperties(format);
}
```

`vk::FormatProperties` 结构体包含三个字段：

- `linearTilingFeatures`：linear tiling 支持的用例
- `optimalTilingFeatures`：optimal tiling 支持的用例
- `bufferFeatures`：buffer 支持的用例

这里只有前两个相关，我们检查哪一个取决于函数的 `tiling` 参数：

```cpp
if (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features) {
    return format;
}
if (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features) {
    return format;
}
```

如果所有候选 format 都不支持所需的用途，我们可以返回一个特殊值或直接抛出异常：

```cpp
vk::Format findSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features) {
    for (const auto format : candidates) {
        vk::FormatProperties props = physicalDevice.getFormatProperties(format);
        if (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features) {
            return format;
        }
        if (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }
    throw std::runtime_error("failed to find supported format!");
}
```

现在我们将使用这个函数创建一个 `findDepthFormat` 辅助函数，用于选择一个包含深度分量且支持用作 depth attachment 的 format：

```cpp
vk::Format findDepthFormat() {
   return findSupportedFormat(
        {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
            vk::ImageTiling::eOptimal,
            vk::FormatFeatureFlagBits::eDepthStencilAttachment
        );
}
```

在这种情况下，确保使用 `vk::FormatFeatureFlagBits` 而不是 `vk::ImageUsageFlagBits`。所有这些候选 format 都包含深度分量，但后两个还包含 stencil 分量。我们暂时不会使用它，但在对这些 format 的 image 执行 layout 转换时确实需要考虑到这一点。

添加一个简单的辅助函数，告诉我们所选的深度 format 是否包含 stencil 分量：

```cpp
bool hasStencilComponent(vk::Format format) {
    return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint;
}
```

从 `createDepthResources` 中调用函数来查找深度 format：

```cpp
vk::Format depthFormat = findDepthFormat();
```

现在我们有了调用 `createImage` 和 `createImageView` 辅助函数所需的全部信息：

```cpp
createImage(swapChainExtent.width, swapChainExtent.height, depthFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal, depthImage, depthImageMemory);
depthImageView = createImageView(depthImage, depthFormat, vk::ImageAspectFlagBits::eDepth);
```

然而，`createImageView` 函数目前假设 subresource 始终为 `vk::ImageAspectFlagBits::eColor`，因此我们需要将该字段变为参数：

```cpp
vk::raii::ImageView createImageView(vk::raii::Image& image, vk::Format format, vk::ImageAspectFlags aspectFlags) {
    ...
    vk::ImageViewCreateInfo viewInfo{
                ...
                .subresourceRange = {aspectFlags, 0, 1, 0, 1}};
    ...
}
```

更新所有对此函数的调用，使用正确的 aspect：

```cpp
swapChainImageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat, vk::ImageAspectFlagBits::eColor);
...
depthImageView = createImageView(depthImage, depthFormat, vk::ImageAspectFlagBits::eDepth);
...
textureImageView = createImageView(textureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor);
```

depth image 的创建到此完成。我们不需要映射它或将另一个 image 复制到它，因为我们将在 command buffer 开始时像 color attachment 一样清除它。

---

## Command Buffer

### 清除值（Clear values）

由于我们现在有多个将被 `vk::AttachmentLoadOp::eClear` 清除的 attachment（颜色和深度），我们还需要指定多个清除值。进入 `recordCommandBuffer`，创建并添加一个额外的 `vk::ClearValue` 变量 `clearDepth`：

```cpp
vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
vk::ClearValue clearDepth = vk::ClearDepthStencilValue(1.0f, 0);
```

Vulkan 中 depth buffer 的深度范围是 0.0 到 1.0，其中 1.0 位于远裁剪面，0.0 位于近裁剪面。depth buffer 中每个点的初始值应该是尽可能远的深度，即 1.0。

### Dynamic Rendering

现在我们的 depth image 已经设置好，需要在 `recordCommandBuffer` 中使用它。这将作为 dynamic rendering 的一部分，类似于设置颜色输出 image。

首先为 depth image 指定一个新的 rendering attachment：

```cpp
vk::RenderingAttachmentInfo depthAttachmentInfo = {
    .imageView   = depthImageView,
    .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
    .loadOp      = vk::AttachmentLoadOp::eClear,
    .storeOp     = vk::AttachmentStoreOp::eDontCare,
    .clearValue  = clearDepth};
```

并将其添加到 dynamic rendering info 结构体中：

```cpp
vk::RenderingInfo renderingInfo = {
    ...
    .pDepthAttachment     = &depthAttachmentInfo};
```

### 显式转换 Depth Image 的 Layout（Explicitly transitioning the depth image）

就像 color attachment 一样，depth attachment 需要处于正确的 layout 才能用于预期的用途。为此，我们需要额外发出 barrier，以确保 depth image 在渲染期间可以用作 depth attachment。

depth image 首先在 early fragment test pipeline 阶段被访问，由于我们有一个清除操作的 load operation，我们应该为写入指定 access mask。

由于我们现在处理了额外的 image 类型（深度），首先为 `transition_image_layout` 函数添加一个新的 image aspect 参数：

```cpp
void transition_image_layout(
    ...
    vk::ImageAspectFlags    image_aspect_flags)
{
    vk::ImageMemoryBarrier2 barrier = {
        ...
        .subresourceRange    = {
            .aspectMask     = image_aspect_flags,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1}};
}
```

现在在 `recordCommandBuffer` 中 command buffer 开始处添加新的 image layout 转换：

```cpp
commandBuffers[currentFrame].begin({});

// 用于 color attachment 的转换
transition_image_layout(
    ...
    vk::ImageAspectFlagBits::eColor);

// 新增的 depth image 转换
transition_image_layout(
    *depthImage,
    vk::ImageLayout::eUndefined,
    vk::ImageLayout::eDepthAttachmentOptimal,
    vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
    vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
    vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
    vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
    vk::ImageAspectFlagBits::eDepth);
```

与 color image 不同，这里我们不需要多个 barrier。由于帧结束后我们不关心 depth attachment 的内容，我们始终可以从 `vk::ImageLayout::eUndefined` 转换过来。这种 layout 的特殊之处在于，你可以始终将其用作源 layout，而无需关心之前发生了什么。

同时确保你调整所有其他对 `transition_image_layout` 函数的调用，以传递正确的 image aspect：

```cpp
// 在开始渲染之前，将 swapchain image 转换为 COLOR_ATTACHMENT_OPTIMAL
transition_image_layout(
    ...
    // color image 也需要指定这个
    vk::ImageAspectFlagBits::eColor);
```

---

## 深度和模板状态（Depth and stencil state）

depth attachment 现在已经可以使用了，但深度测试仍然需要在 graphics pipeline 中启用。它通过 `PipelineDepthStencilStateCreateInfo` 结构体配置：

```cpp
vk::PipelineDepthStencilStateCreateInfo depthStencil{
    .depthTestEnable       = vk::True,
    .depthWriteEnable      = vk::True,
    .depthCompareOp        = vk::CompareOp::eLess,
    .depthBoundsTestEnable = vk::False,
    .stencilTestEnable     = vk::False};
```

`depthTestEnable` 字段指定是否应将新片元的深度与 depth buffer 进行比较，以决定是否丢弃它们。`depthWriteEnable` 字段指定通过深度测试的新片元的深度是否应实际写入 depth buffer。`depthCompareOp` 字段指定执行保留或丢弃片元的比较操作。我们遵循"深度值越小越近"的约定，所以新片元的深度应该更小（less）。

`depthBoundsTestEnable`、`minDepthBounds` 和 `maxDepthBounds` 字段用于可选的深度边界测试。基本上，这允许你只保留落在指定深度范围内的片元。我们不会使用此功能。

最后三个字段配置 stencil buffer 操作，在本教程中我们也不会使用。如果你想使用这些操作，则必须确保 depth/stencil image 的 format 包含 stencil 分量。

如果 dynamic rendering 设置包含 depth stencil attachment，则必须始终指定 depth stencil state。

更新 `pipelineCreateInfoChain` 结构体链，以引用我们刚刚填写的 depth stencil state，并添加对我们使用的深度 format 的引用：

```cpp
vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain = {
    {.stageCount          = 2,
        ...
        .pDepthStencilState  = &depthStencil,
        ...
    {.colorAttachmentCount = 1, .pColorAttachmentFormats = &swapChainSurfaceFormat.format, .depthAttachmentFormat = depthFormat}};
```

如果你现在运行程序，应该看到几何体的片元现在已正确排序：

![depth correct](https://docs.vulkan.org/tutorial/latest/_images/images/depth_correct.png)

---

## 处理窗口缩放（Handling window resize）

当窗口大小改变时，depth buffer 的分辨率应该随之改变，以匹配新的 color attachment 分辨率。扩展 `recreateSwapChain` 函数，在这种情况下重新创建 depth 资源：

```cpp
void recreateSwapChain() {
    int width = 0, height = 0;
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }
    device.waitIdle(device);
    cleanupSwapChain();
    createSwapChain();
    createImageViews();
    createDepthResources();
}
```

恭喜，你的应用程序现在终于可以渲染任意的 3D 几何体并使其看起来正确了。我们将在下一章通过绘制一个带纹理的模型来尝试这一点！
