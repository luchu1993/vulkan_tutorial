# 结论（Conclusion）

> 原文：[https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/02_Graphics_pipeline_basics/04_Conclusion.html](https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/02_Graphics_pipeline_basics/04_Conclusion.html)

现在我们可以将前几章中所有的结构体和对象组合起来，创建 graphics pipeline！以下是我们目前拥有的对象类型，简要回顾一下：

- **Shader stages**：定义 graphics pipeline 可编程阶段功能的 shader module
- **Fixed-function state**：定义 pipeline 固定功能阶段的所有结构体，例如 input assembly、rasterizer、viewport 和 color blending
- **Pipeline layout**：shader 引用的 uniform 和 push 值，可在绘制时更新
- **Dynamic rendering**：渲染期间将使用的 attachment 格式

所有这些组合在一起就完整定义了 graphics pipeline 的功能，因此现在我们可以开始填写 `vk::GraphicsPipelineCreateInfo` 结构体了。首先引用 shader stage 信息：

```cpp
vk::GraphicsPipelineCreateInfo pipelineInfo({}, 2, shaderStages);
```

然后引用所有固定功能阶段的结构体：

```cpp
vk::GraphicsPipelineCreateInfo pipelineInfo({}, 2, shaderStages, &vertexInputInfo,
    &inputAssembly, {}, &viewportState, &rasterizer, &multisampling, {}, &colorBlending,
    &dynamicState);
```

之后是 pipeline layout，它是一个 Vulkan handle 而非结构体指针：

```cpp
vk::GraphicsPipelineCreateInfo pipelineInfo({}, 2, shaderStages, &vertexInputInfo,
    &inputAssembly, {}, &viewportState, &rasterizer, &multisampling, {}, &colorBlending,
    &dynamicState, *pipelineLayout);
```

最后，我们使用 dynamic rendering 而非传统的 render pass。我们通过 `pNext` 链传递一个 `vk::PipelineRenderingCreateInfo` 结构体，并将 `renderPass` 设置为 `nullptr`：

```cpp
vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo{
    .colorAttachmentCount    = 1,
    .pColorAttachmentFormats = &swapChainImageFormat
};

vk::GraphicsPipelineCreateInfo pipelineInfo{
    .pNext               = &pipelineRenderingCreateInfo,
    .stageCount          = 2,
    .pStages             = shaderStages,
    .pVertexInputState   = &vertexInputInfo,
    .pInputAssemblyState = &inputAssembly,
    .pViewportState      = &viewportState,
    .pRasterizationState = &rasterizer,
    .pMultisampleState   = &multisampling,
    .pColorBlendState    = &colorBlending,
    .pDynamicState       = &dynamicState,
    .layout              = pipelineLayout,
    .renderPass          = nullptr
};
```

还有两个参数：`basePipelineHandle` 和 `basePipelineIndex`。Vulkan 允许你通过从现有 pipeline 派生来创建新的 graphics pipeline。pipeline 派生（pipeline derivatives）的思路是：当新 pipeline 与现有 pipeline 有大量共同功能时，设置成本更低，并且在同一父 pipeline 的子 pipeline 之间切换也可以更快。你可以通过 `basePipelineHandle` 指定现有 pipeline 的 handle，或者通过 `basePipelineIndex` 引用即将通过索引创建的另一个 pipeline。目前只有一个 pipeline，所以只需指定一个 null handle 和一个无效的索引。仅当在 `vk::GraphicsPipelineCreateInfo` 的 `flags` 字段中同时指定了 `vk::PipelineCreateFlagBits::eDerivative` 标志时，这些值才会被使用：

```cpp
pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
pipelineInfo.basePipelineIndex  = -1;
```

现在通过创建一个 `vk::raii::Pipeline` 类型的类成员来保存该对象，并做好准备：

```cpp
vk::raii::Pipeline graphicsPipeline = nullptr;
```

最后创建 graphics pipeline：

```cpp
graphicsPipeline = vk::raii::Pipeline(device, nullptr, pipelineInfo);
```

`vk::raii::Pipeline` 构造函数的第二个参数是可选的 `vk::PipelineCache` 对象。pipeline cache 可用于在多次调用 `vkCreateGraphicsPipelines` 时存储和复用与 pipeline 创建相关的数据，如果将其保存到文件中，甚至可以跨程序执行复用。这使得在稍后的时间点显著加速 pipeline 创建成为可能。我们将在 pipeline cache 章节中对此进行探讨。

graphics pipeline 是所有常见绘制操作所必需的，因此它也应该在程序结束时被销毁，由 `vk::raii::Pipeline` 的析构函数在清理时自动完成。

现在运行你的程序，确认成功创建了 pipeline！我们已经非常接近在屏幕上看到内容了。在接下来的几章中，我们将从 swap chain image 中设置实际的 framebuffer，并准备绘制命令。

**[C++ 代码](https://docs.vulkan.org/tutorial/latest/_attachments/12_graphics_pipeline_complete.cpp)**
