# 帧缓冲（Framebuffers / Dynamic Rendering）

> 原文：[https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/03_Drawing/00_Framebuffers.html](https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/03_Drawing/00_Framebuffers.html)

在早期版本的 Vulkan 中，我们需要创建 framebuffer 来将 image view 绑定到 render pass。然而，随着 Vulkan 1.3 引入 dynamic rendering，我们现在可以直接渲染到 image view，而无需创建 framebuffer 或 render pass。

## 动态渲染简介（Introduction to Dynamic Rendering）

Dynamic rendering 通过消除对 render pass 和 framebuffer 对象的需求，简化了渲染工作流程。我们可以在开始渲染时直接指定 color、depth 和 stencil attachment。

实现中使用 `vk::RenderingAttachmentInfo` 和 `vk::RenderingInfo` 结构体来指定 attachment 和渲染参数。

## 使用 Dynamic Rendering 录制 Command Buffer

下一章将介绍 command buffer 和渲染命令。以下是使用 dynamic rendering 的预览：
```cpp
void recordCommandBuffer(uint32_t imageIndex) {
    commandBuffer.begin({});

    // Transition the image layout for rendering
    transition_image_layout(
        imageIndex,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eColorAttachmentOptimal,
        {},
        vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput
    );

    // Set up the color attachment
    vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
    vk::RenderingAttachmentInfo attachmentInfo = {
        .imageView = swapChainImageViews[imageIndex],
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .clearValue = clearColor
    };

    // Set up the rendering info
    vk::RenderingInfo renderingInfo = {
        .renderArea = { .offset = { 0, 0 }, .extent = swapChainExtent },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &attachmentInfo
    };

    // Begin rendering
    commandBuffer.beginRendering(renderingInfo);

    // Rendering commands will go here

    // End rendering
    commandBuffer.endRendering();

    // Transition the image layout for presentation
    transition_image_layout(
        imageIndex,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageLayout::ePresentSrcKHR,
        vk::AccessFlagBits2::eColorAttachmentWrite,
        {},
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::PipelineStageFlagBits2::eBottomOfPipe
    );

    commandBuffer.end();
}
```

该示例展示了图像布局转换（image layout transition）、使用 `vk::RenderingAttachmentInfo` 设置 color attachment 信息（指定 image view、layout、load/store 操作和 clear value），以及将参数组合到 `vk::RenderingInfo` 中。代码调用 `commandBuffer.beginRendering()` 和 `commandBuffer.endRendering()`，而不使用 render pass。

这种方式无需创建 framebuffer 或 render pass，为现代渲染技术提供了更大的灵活性。

下一章将介绍如何创建 command buffer 并使用 dynamic rendering 编写初始绘制命令。

[C++ 代码](../../_attachments/14_command_buffers.cpp) / [Slang shader](../../_attachments/09_shader_base.slang) / [GLSL Vertex shader](../../_attachments/09_shader_base.vert) / [GLSL Fragment shader](../../_attachments/09_shader_base.frag)
