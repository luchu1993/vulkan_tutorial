# Dynamic Rendering（Dynamic Rendering）

> 原文：https://docs.vulkan.org/tutorial/latest/courses/18_Ray_tracing/01_Dynamic_rendering.html

## 目标（Objective）

确保基础项目使用 dynamic rendering，并了解如何使用 RenderDoc 进行验证。

在 dynamic rendering 中，我们不再创建 `VkRenderPass` 或 `VkFrameBuffer`；取而代之的是使用 `vkCmdBeginRenderingKHR` 开始渲染，并即时指定 attachment。这使我们的代码更加灵活（无需预先声明 subpass），现在已成为 Vulkan 中"现代"的渲染方式。

---

## Task 1：检查 dynamic rendering 的设置（Check the setup for dynamic rendering）

在提供的代码库中，找到图形管线的初始化部分：

```cpp
/* TASK01: Check the setup for dynamic rendering
 *
 * This new struct replaces what previously was the render pass in the pipeline creation.
 * Note how this structure is now linked in .pNext below, and .renderPass is not used.
 */
vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo{
    .colorAttachmentCount = 1,
    .pColorAttachmentFormats = &swapChainImageFormat,
    .depthAttachmentFormat = depthFormat
};
vk::GraphicsPipelineCreateInfo pipelineInfo{
    .pNext = &pipelineRenderingCreateInfo,
    .stageCount = 2,
    .pStages = shaderStages,
    .pVertexInputState = &vertexInputInfo,
    .pInputAssemblyState = &inputAssembly,
    .pViewportState = &viewportState,
    .pRasterizationState = &rasterizer,
    .pMultisampleState = &multisampling,
    .pDepthStencilState = &depthStencil,
    .pColorBlendState = &colorBlending,
    .pDynamicState = &dynamicState,
    .layout = pipelineLayout,
    .renderPass = nullptr
};
graphicsPipeline = vk::raii::Pipeline(device, nullptr, pipelineInfo);
```

以及之后录制命令缓冲区时开始渲染的部分：

```cpp
/* TASK01: Check the setup for dynamic rendering
 *
 * With dynamic rendering, we specify the image view and load/store operations directly
 * in the vk::RenderingAttachmentInfo structure.
 * This approach eliminates the need for explicit render pass and framebuffer objects,
 * simplifying the code and providing flexibility to change attachments at runtime.
 */
vk::RenderingAttachmentInfo colorAttachmentInfo = {
    .imageView = swapChainImageViews[imageIndex],
    .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
    .loadOp = vk::AttachmentLoadOp::eClear,
    .storeOp = vk::AttachmentStoreOp::eStore,
    .clearValue = clearColor
};
vk::RenderingAttachmentInfo depthAttachmentInfo = {
    .imageView = depthImageView,
    .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
    .loadOp = vk::AttachmentLoadOp::eClear,
    .storeOp = vk::AttachmentStoreOp::eDontCare,
    .clearValue = clearDepth
};
// The vk::RenderingInfo structure combines these attachments with other rendering parameters.
vk::RenderingInfo renderingInfo = {
    .renderArea = { .offset = { 0, 0 }, .extent = swapChainExtent },
    .layerCount = 1,
    .colorAttachmentCount = 1,
    .pColorAttachments = &colorAttachmentInfo,
    .pDepthAttachment = &depthAttachmentInfo
};
// Note: .beginRendering replaces the previous .beginRenderPass call.
commandBuffers[frameIndex].beginRendering(renderingInfo);
```

更多背景信息请参阅上一个教程章节。

---

## 使用 RenderDoc 验证 Dynamic Rendering（Dynamic rendering with RenderDoc）

使用 RenderDoc 启动应用程序并捕获一帧：

1. 指定可执行文件路径：`Vulkan-Tutorial\attachments\build\38_ray_tracing\Debug\38_ray_tracing.exe`
2. 指定工作目录：`Vulkan-Tutorial\attachments\build\38_ray_tracing`
3. 启动应用程序。

在 Event Browser 中，你应该能看到确认 dynamic rendering 已正确设置的调用：

- `vkCmdBeginRenderingKHR` 和 `vkCmdEndRenderingKHR`
- `VkRenderingInfoKHR` 替代了旧的 render pass/framebuffer 概念
- 通过 `VkRenderingAttachmentInfo` 设置颜色（和深度）attachment

![RenderDoc launch](https://docs.vulkan.org/tutorial/latest/_images/images/38_TASK01_renderdoc_launch.png)

在 RenderDoc 的 Texture Viewer 中，你可以在各个时间点检查颜色和深度 attachment：

![RenderDoc events](https://docs.vulkan.org/tutorial/latest/_images/images/38_TASK01_renderdoc_events.png)

![RenderDoc color](https://docs.vulkan.org/tutorial/latest/_images/images/38_TASK01_renderdoc_color.gif)

Dynamic rendering 减少了 CPU 开销，并且通过 `VK_KHR_dynamic_rendering_local_read` 扩展，让你无需完整的 render pass 就能进行类似 subpass 的 tile 本地读取。这对于延迟渲染等技术非常有用，尤其在移动端 tile-based GPU 上，可以在 tile 内读取上一个 pass 的 attachment 数据而无需额外的内存带宽。虽然我们这里不实现延迟渲染器，但请记住这一优势对移动端的意义。

完成此步骤后，你应该确信 dynamic rendering 已正确设置。现在我们可以继续学习光线追踪功能了。
