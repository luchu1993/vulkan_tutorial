# 固定功能（Fixed functions）

> 原文：[https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/02_Graphics_pipeline_basics/02_Fixed_functions.html](https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/02_Graphics_pipeline_basics/02_Fixed_functions.html)

旧版图形 API 为 graphics pipeline 的大多数阶段提供了默认状态。在 Vulkan 中，你必须对大多数 pipeline 状态进行显式设置，因为这些状态将被烘焙（baked）到一个不可变的 pipeline 状态对象中。在本章中，我们将填写所有这些结构体，以配置这些固定功能操作。

## 动态状态（Dynamic state）

虽然大多数 pipeline 状态必须烘焙到 pipeline 对象中，但实际上有少量状态可以在不重建 pipeline 的情况下在绘制时进行更改。例如 viewport 的大小、line width 以及 blend constants。如果你想使用动态状态并保留这些属性，则必须像下面这样填写一个 `vk::PipelineDynamicStateCreateInfo` 结构体：

```cpp
std::vector<vk::DynamicState> dynamicStates = {
    vk::DynamicState::eViewport,
    vk::DynamicState::eScissor
};

vk::PipelineDynamicStateCreateInfo dynamicState{
    .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
    .pDynamicStates = dynamicStates.data()
};
```

这将导致这些值的配置被忽略，你将需要（并且有义务）在绘制时指定这些数据。这种方式更加灵活，对于 viewport 和 scissor 状态来说非常常见，因为将它们作为静态状态烘焙会导致设置更复杂。

## 顶点输入（Vertex input）

`vk::PipelineVertexInputStateCreateInfo` 结构体描述了将传递给 vertex shader 的顶点数据的格式。它大致以两种方式描述顶点数据：

- **Bindings**：数据之间的间距，以及数据是逐顶点（per-vertex）还是逐实例（per-instance）的（参见 instancing）
- **Attribute descriptions**：传递给 vertex shader 的属性的类型，从哪个 binding 加载它们以及以什么偏移量（offset）加载

由于我们直接在 vertex shader 中对顶点数据进行了硬编码，我们将填写这个结构体，指定目前没有需要加载的顶点数据。我们将在 vertex buffer 章节中重新讨论它。

```cpp
vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
```

`pVertexBindingDescriptions` 和 `pVertexAttributeDescriptions` 成员指向描述顶点数据加载细节的结构体数组。将其添加到 `createGraphicsPipeline` 函数中的 `shaderStages` 数组之后。

## 输入装配（Input assembly）

`vk::PipelineInputAssemblyStateCreateInfo` 结构体描述了两件事：将从顶点绘制何种几何图形，以及是否应启用图元重启（primitive restart）。前者由 `topology` 成员指定，可以有以下几种值：

- `vk::PrimitiveTopology::ePointList`：由顶点构成的点
- `vk::PrimitiveTopology::eLineList`：每 2 个顶点构成一条线，顶点不复用
- `vk::PrimitiveTopology::eLineStrip`：每条线的终点被用作下一条线的起点
- `vk::PrimitiveTopology::eTriangleList`：每 3 个顶点构成一个三角形，顶点不复用
- `vk::PrimitiveTopology::eTriangleStrip`：每个三角形的第二、第三个顶点被用作下一个三角形的前两个顶点

通常情况下，顶点从 vertex buffer 中按索引顺序加载，但通过 element buffer，你可以自己指定使用的索引。这允许你复用顶点、进行顶点缓存优化等操作。如果将 `primitiveRestartEnable` 成员设置为 `vk::True`，则可以通过使用特殊索引 `0xFFFF` 或 `0xFFFFFFFF` 在 `_STRIP` 拓扑模式中分解线段和三角形。

本教程我们打算绘制三角形，因此沿用以下数据结构：

```cpp
vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
    .topology = vk::PrimitiveTopology::eTriangleList
};
```

## Viewport 与 scissor

Viewport 基本上描述了输出将被渲染到的 framebuffer 区域。它几乎总是从 `(0, 0)` 到 `(width, height)`，在本教程中也是如此。

```cpp
vk::Viewport viewport{
    0.0f,
    0.0f,
    static_cast<float>(swapChainExtent.width),
    static_cast<float>(swapChainExtent.height),
    0.0f,
    1.0f
};
```

请记住，swap chain 及其 image 的大小可能与窗口的 `WIDTH` 和 `HEIGHT` 不同。Swap chain image 稍后将被用作 framebuffer，因此我们应使用其大小。

`minDepth` 和 `maxDepth` 值指定了 framebuffer 使用的深度值范围。这些值必须在 `[0.0f, 1.0f]` 范围内，但 `minDepth` 可以高于 `maxDepth`。如果你不做什么特殊操作，则应使用标准值 `0.0f` 和 `1.0f`。

Viewport 定义了从 image 到 framebuffer 的变换，而 scissor rectangle 则定义了实际存储像素的区域。Scissor rectangle 之外的任何像素都会被 rasterizer 丢弃。它的功能类似于过滤器，而非变换。区别如下图所示。注意，左侧的 scissor rectangle 只是许多可能产生该图像的选项之一，只要它大于 viewport 即可。

![Viewport 与 Scissor 对比](https://docs.vulkan.org/tutorial/latest/_images/images/viewports_scissors.png)

在本教程中，我们希望绘制到整个 framebuffer，因此指定一个完全覆盖它的 scissor rectangle：

```cpp
vk::Rect2D scissor{
    vk::Offset2D{ 0, 0 },
    swapChainExtent
};
```

Viewport 和 scissor rectangle 既可以作为 pipeline 的静态部分指定，也可以作为动态状态在 command buffer 中设置。虽然前者更符合其他状态的设置方式，但将 viewport 和 scissor 状态设为动态通常更为方便，因为它赋予了你更多灵活性。这非常常见，所有实现都必须支持这种动态状态，无需额外的性能开销。

当选择动态 viewport 和 scissor rectangle 时，需要启用相应的动态状态：

```cpp
std::vector<vk::DynamicState> dynamicStates = {
    vk::DynamicState::eViewport,
    vk::DynamicState::eScissor
};

vk::PipelineDynamicStateCreateInfo dynamicState{
    .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
    .pDynamicStates = dynamicStates.data()
};
```

然后只需在创建时指定它们的数量：

```cpp
vk::PipelineViewportStateCreateInfo viewportState{
    .viewportCount = 1,
    .scissorCount = 1
};
```

实际的 viewport 和 scissor rectangle 将在绘制时设置。

使用动态状态可以让你更灵活地指定不同的 viewport 和/或 scissor rectangle。但这也意味着你必须在 pipeline 中同时拥有多个 viewport 和 scissor rectangle，这需要 GPU 特性的支持（参见逻辑设备创建）。

## Rasterizer

Rasterizer 接收 vertex shader 输出的经过几何变换的顶点，并将其转换为 fragment，随后由 fragment shader 进行着色。它还执行深度测试（depth testing）、面剔除（face culling）和 scissor 测试，并可以配置为输出填充整个多边形的 fragment，还是只输出边缘（wireframe 渲染）。所有这些都通过 `vk::PipelineRasterizationStateCreateInfo` 结构体配置。

```cpp
vk::PipelineRasterizationStateCreateInfo rasterizer{
    .depthClampEnable        = vk::False,
    .rasterizerDiscardEnable = vk::False,
    .polygonMode             = vk::PolygonMode::eFill,
    .cullMode                = vk::CullModeFlagBits::eBack,
    .frontFace               = vk::FrontFace::eClockwise,
    .depthBiasEnable         = vk::False,
    .lineWidth               = 1.0f
};
```

如果 `depthClampEnable` 设置为 `vk::True`，超出近裁剪面和远裁剪面范围的 fragment 会被夹紧（clamp）而不是丢弃。这在某些特殊情况下（如 shadow map）很有用。使用此功能需要启用对应的 GPU 特性。

如果 `rasterizerDiscardEnable` 设置为 `vk::True`，geometry 永远不会通过 rasterizer 阶段，这将禁止任何输出写入 framebuffer。

`polygonMode` 决定如何从 geometry 生成 fragment，可选项有以下几种：

- `vk::PolygonMode::eFill`：用 fragment 填充多边形区域
- `vk::PolygonMode::eLine`：多边形边缘绘制为线段
- `vk::PolygonMode::ePoint`：多边形顶点绘制为点

使用除 fill 以外的其他模式需要启用对应的 GPU 特性。

`lineWidth` 成员很直观，它描述了线段的粗细（以片元数量计）。支持的最大 line width 取决于硬件，任何大于 `1.0f` 的 line width 都需要启用 `wideLines` GPU 特性。

`cullMode` 变量决定使用的面剔除类型。你可以禁用剔除、剔除正面（front face）、剔除背面（back face），或者两者都剔除。`frontFace` 变量指定被视为正面的顶点顺序，可以是顺时针（clockwise）或逆时针（counter-clockwise）。

`depthBiasEnable`、`depthBiasConstantFactor`、`depthBiasClamp` 和 `depthBiasSlopeFactor` 这些参数有时用于 shadow mapping，但我们这里不使用它们，因此将 `depthBiasEnable` 设置为 `vk::False`。

## Multisampling

`vk::PipelineMultisampleStateCreateInfo` 结构体配置 multisampling，这是实现抗锯齿（anti-aliasing）的方法之一。它通过组合多个多边形中 rasterization 到同一像素的 fragment shader 结果来实现。这主要发生在边缘处，也是最明显的锯齿现象出现的地方。由于在只有一个多边形映射到某个像素时不需要多次运行 fragment shader，与简单地渲染到更高分辨率再降采样相比，它的性能开销小得多。启用它需要启用对应的 GPU 特性。

```cpp
vk::PipelineMultisampleStateCreateInfo multisampling{
    .rasterizationSamples = vk::SampleCountFlagBits::e1,
    .sampleShadingEnable  = vk::False
};
```

在本教程的后面章节，我们将重新讨论 multisampling，现在先禁用它。

## 深度和模板测试（Depth and stencil testing）

如果你使用深度和/或 stencil buffer，还需要通过 `vk::PipelineDepthStencilStateCreateInfo` 配置深度和 stencil 测试。我们目前没有使用，所以可以简单地传递 `nullptr` 而不是指向该结构体的指针。我们将在深度缓冲章节重新讨论它。

## 颜色混合（Color blending）

Fragment shader 返回颜色之后，需要将其与 framebuffer 中已有的颜色进行合并。这个变换过程被称为 color blending，有两种方式可以实现：

- 将旧值与新值混合，产生最终颜色
- 使用 bitwise 操作合并旧值与新值

有两种类型的结构体用于配置 color blending。第一种是 `vk::PipelineColorBlendAttachmentState`，包含每个绑定的 framebuffer 的配置。第二种是 `vk::PipelineColorBlendStateCreateInfo`，包含全局 color blending 设置。在我们的例子中，只有一个 framebuffer：

```cpp
vk::PipelineColorBlendAttachmentState colorBlendAttachment{
    .blendEnable    = vk::False,
    .colorWriteMask = vk::ColorComponentFlagBits::eR |
                      vk::ColorComponentFlagBits::eG |
                      vk::ColorComponentFlagBits::eB |
                      vk::ColorComponentFlagBits::eA
};
```

这个每个 framebuffer 的结构体允许你配置第一种 color blending 方式。其操作通过以下伪代码来最好地说明：

```
if (blendEnable) {
    finalColor.rgb = (srcColorBlendFactor * newColor.rgb) <colorBlendOp> (dstColorBlendFactor * oldColor.rgb);
    finalColor.a = (srcAlphaBlendFactor * newColor.a) <alphaBlendOp> (dstAlphaBlendFactor * oldColor.a);
} else {
    finalColor = newColor;
}

finalColor = finalColor & colorWriteMask;
```

如果 `blendEnable` 为 `vk::False`，则来自 fragment shader 的新颜色将不加修改地直接传递。否则，执行两次混合操作来计算新颜色。最终颜色会与 `colorWriteMask` 进行 AND 操作，以确定哪些通道实际通过。

最常见的使用 color blending 的方式是实现 alpha blending，我们希望根据不透明度将新颜色与旧颜色混合。`finalColor` 应按如下方式计算：

```
finalColor.rgb = newAlpha * newColor + (1 - newAlpha) * oldColor;
finalColor.a = newAlpha.a;
```

这可以通过以下参数实现：

```cpp
vk::PipelineColorBlendAttachmentState colorBlendAttachment{
    .blendEnable         = vk::True,
    .srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
    .dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
    .colorBlendOp        = vk::BlendOp::eAdd,
    .srcAlphaBlendFactor = vk::BlendFactor::eOne,
    .dstAlphaBlendFactor = vk::BlendFactor::eZero,
    .alphaBlendOp        = vk::BlendOp::eAdd,
    .colorWriteMask      = vk::ColorComponentFlagBits::eR |
                           vk::ColorComponentFlagBits::eG |
                           vk::ColorComponentFlagBits::eB |
                           vk::ColorComponentFlagBits::eA
};
```

你可以在 Vulkan 规范的 `VkBlendFactor` 和 `VkBlendOp` 枚举中找到所有可能的操作。

第二种结构体引用所有 framebuffer 的结构体数组，并允许你设置混合常量，以在上述计算中用作混合因子：

```cpp
vk::PipelineColorBlendStateCreateInfo colorBlending{
    .logicOpEnable   = vk::False,
    .logicOp         = vk::LogicOp::eCopy,
    .attachmentCount = 1,
    .pAttachments    = &colorBlendAttachment
};
```

如果你想使用第二种 blending 方式（bitwise combination），应将 `logicOpEnable` 设置为 `vk::True`。然后可以在 `logicOp` 字段中指定 bitwise 操作。请注意，这将自动禁用第一种方式，就好像你对每个绑定的 framebuffer 都将 `blendEnable` 设置为 `vk::False`！`colorWriteMask` 在此模式下也会生效，以确定 framebuffer 中哪些通道实际受到影响。也可以同时禁用这两种模式，此时 fragment 的颜色将不加修改地写入 framebuffer。

## Pipeline layout

你可以在 shader 中使用 `uniform` 值，这些全局变量类似于动态状态变量，可以在绘制时更改以改变 shader 的行为，而无需重建 shader。它们通常用于将变换矩阵传递给 vertex shader，或在 fragment shader 中创建纹理采样器。

这些 uniform 值需要在 pipeline 创建期间通过创建 `vk::PipelineLayout` 对象来指定。即使我们在未来的章节中才会用到它，现在也需要创建一个空的 pipeline layout。

创建一个类成员来保存该对象，因为我们稍后将从其他函数中引用它：

```cpp
vk::raii::PipelineLayout pipelineLayout = nullptr;
```

然后在 `createGraphicsPipeline` 函数中创建该对象：

```cpp
vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
    .setLayoutCount         = 0,
    .pushConstantRangeCount = 0
};

pipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);
```

该结构体还指定了 push constants，这是将动态值传递给 shader 的另一种方式，我们可能会在未来的章节中介绍。Pipeline layout 将在整个程序生命周期中被引用，因此应在最后销毁它：

```cpp
void cleanup() {
    pipelineLayout.clear();
    ...
}
```

## 总结

以上就是所有固定功能状态的全部内容！从头设置这一切确实需要大量工作，但好处是我们现在几乎完全了解 graphics pipeline 中发生的一切！这降低了因某些组件的默认状态与你预期不符而遇到意外行为的可能性。

然而，在最终创建 graphics pipeline 之前，还有一个对象需要创建，那就是 render pass。

**[C++ 代码](https://docs.vulkan.org/tutorial/latest/_attachments/10_fixed_functions.cpp)**
