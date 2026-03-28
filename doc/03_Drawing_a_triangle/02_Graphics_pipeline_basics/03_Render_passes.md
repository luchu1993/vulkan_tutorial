# 渲染流程（Render passes）

> 原文：[https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/02_Graphics_pipeline_basics/03_Render_passes.html](https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/02_Graphics_pipeline_basics/03_Render_passes.html)

## 简介（Introduction）

在 Vulkan 1.3 之前，在完成 pipeline 创建之前，我们需要通过 render pass 对象来告知 Vulkan 在渲染时将使用哪些 framebuffer attachment。然而，随着 Vulkan 1.3 中动态渲染（dynamic rendering）的引入，我们现在可以在创建 graphics pipeline 时以及录制 command buffer 时直接指定这些信息。

Dynamic rendering 通过消除对 render pass 和 framebuffer 对象的需求，简化了渲染流程。我们可以直接在开始渲染时指定颜色（color）、深度（depth）和 stencil attachment，而无需预先创建这些对象。

## Pipeline Rendering Create Info

为了使用 dynamic rendering，我们需要在创建 graphics pipeline 时指定渲染期间将使用的 attachment 格式。这通过 `vk::PipelineRenderingCreateInfo` 结构体完成：

```cpp
vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo{
    .colorAttachmentCount    = 1,
    .pColorAttachmentFormats = &swapChainImageFormat
};
```

这个结构体指定我们将使用一个格式与 swap chain image 相同的 color attachment。然后我们通过 `pNext` 链将这个结构体包含到 `vk::GraphicsPipelineCreateInfo` 结构体中：

```cpp
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

注意，`renderPass` 参数被设置为 `nullptr`，因为我们使用的是 dynamic rendering 而非传统的 render pass。

## Command Buffer 录制（Command Buffer Recording）

在录制 command buffer 时，我们将使用 `vk::CommandBuffer::beginRendering` 函数来开始向指定的 attachment 进行渲染。我们将在绘制（Drawing）章节中更详细地介绍这一点。

Dynamic rendering 的优势在于，它通过消除对 render pass 和 framebuffer 对象的需求来简化渲染流程。它还提供了更多灵活性，允许我们在不创建新 render pass 对象的情况下更改渲染目标 attachment。

在下一章中，我们将把所有内容组合在一起，最终创建 graphics pipeline 对象！

**[C++ 代码](https://docs.vulkan.org/tutorial/latest/_attachments/11_render_passes.cpp)**
