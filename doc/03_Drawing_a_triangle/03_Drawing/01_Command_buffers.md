# Command Buffer（Command buffers）

> 原文：[https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/03_Drawing/01_Command_buffers.html](https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/03_Drawing/01_Command_buffers.html)

Vulkan 中的命令，例如绘制操作和内存传输，并不是通过函数调用直接执行的。你必须将所有想要执行的操作记录到 command buffer 对象中。这样做的好处是，当我们准备好告诉 Vulkan 我们想要执行什么时，所有命令都会一起提交。由于所有命令都同时可用，Vulkan 可以更高效地处理这些命令。此外，这也允许在需要时在多个线程上进行命令录制。

## Command Pool

在创建 command buffer 之前，我们必须先创建 command pool。Command pool 管理用于存储 buffer 的内存，command buffer 则从 command pool 中分配。添加一个新的类成员来存储 `VkCommandPool`：

```cpp
vk::raii::CommandPool commandPool = nullptr;
```

然后创建一个新的函数 `createCommandPool`，并在 `initVulkan` 中于 graphics pipeline 创建完成后调用它。

```cpp
void initVulkan() {
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
    createGraphicsPipeline();
    createCommandPool();
}

...

void createCommandPool() {

}
```

Command pool 的创建只需要两个参数：

```cpp
vk::CommandPoolCreateInfo poolInfo{ .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer, .queueFamilyIndex = graphicsIndex };
```

Command pool 有两个可用的 flag：

- `VK_COMMAND_POOL_CREATE_TRANSIENT_BIT`：提示 command buffer 会非常频繁地使用新命令重新录制（可能会改变内存分配行为）。
- `VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT`：允许单独重置 command buffer，没有此 flag 时，所有 command buffer 必须一起重置。

我们将在每一帧录制一个 command buffer，因此我们希望能够重置并重新录制它。因此，我们需要为 command pool 设置 `VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT` flag 位。

Command buffer 通过在某个设备队列（device queue）上提交来执行，例如我们检索到的 graphics 队列和 presentation 队列。每个 command pool 只能分配提交到单一类型队列的 command buffer。我们要录制绘制命令，这就是我们选择 graphics queue family 的原因。

```cpp
commandPool = vk::raii::CommandPool(device, poolInfo);
```

使用 `vkCreateCommandPool` 函数完成 command pool 的创建。它没有任何特殊参数。命令将在整个程序运行过程中用于在屏幕上绘制内容。

## Command Buffer 分配

我们现在可以开始分配 command buffer 了。

创建一个 `VkCommandBuffer` 对象作为类成员。Command buffer 会在其 command pool 销毁时自动释放，因此我们不需要显式清理。

```cpp
vk::raii::CommandBuffer commandBuffer = nullptr;
```

我们现在开始编写 `createCommandBuffer` 函数，从 command pool 分配单个 command buffer。

```cpp
void initVulkan() {
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
    createGraphicsPipeline();
    createCommandPool();
    createCommandBuffer();
}

...

void createCommandBuffer() {

}
```

Command buffer 使用 `vkAllocateCommandBuffers` 函数分配，该函数以 `VkCommandBufferAllocateInfo` 结构体作为参数，指定 command pool 和要分配的 buffer 数量：

```cpp
vk::CommandBufferAllocateInfo allocInfo{ .commandPool = commandPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1 };

commandBuffer = std::move(vk::raii::CommandBuffers(device, allocInfo).front());
```

`level` 参数指定分配的 command buffer 是主 command buffer 还是次级 command buffer。

- `VK_COMMAND_BUFFER_LEVEL_PRIMARY`：可以提交到队列执行，但不能从其他 command buffer 调用。
- `VK_COMMAND_BUFFER_LEVEL_SECONDARY`：不能直接提交，但可以从主 command buffer 中调用。

我们在这里不会使用次级 command buffer 功能，但你可以想象它对于复用主 command buffer 中的常见操作很有帮助。

由于我们只分配一个 command buffer，`commandBufferCount` 参数就是 1。

## Command Buffer 录制

我们现在开始编写 `recordCommandBuffer` 函数，它将我们想要执行的命令写入 command buffer 中。要使用的 `VkCommandBuffer` 作为参数传入，同时还有我们想写入的当前 swap chain image 的索引。

```cpp
void recordCommandBuffer(uint32_t imageIndex) {

}
```

我们始终通过调用 `vkBeginCommandBuffer` 开始录制 command buffer，并以一个小的 `VkCommandBufferBeginInfo` 结构体作为参数，指定该特定 command buffer 的使用细节。

```cpp
commandBuffer->begin( {} );
```

`flags` 参数指定我们将如何使用 command buffer。可用的值如下：

- `VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT`：command buffer 将在执行一次后立即重新录制。
- `VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT`：这是一个完全在单个 render pass 内的次级 command buffer。
- `VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT`：command buffer 可以在已经处于待处理执行状态时重新提交。

目前这些 flag 都不适用于我们。

`pInheritanceInfo` 参数仅对次级 command buffer 有意义。它指定从调用它的主 command buffer 继承哪些状态。

如果 command buffer 已经录制过一次，那么调用 `vkBeginCommandBuffer` 会隐式地重置它。之后无法向 buffer 追加命令。

## Image Layout 转换

在开始渲染到图像之前，我们需要将其 layout 转换到适合渲染的状态。在 Vulkan 中，image 可以处于针对不同操作优化的不同 layout。例如，image 可以处于最适合呈现到屏幕的 layout，或者最适合作为 color attachment 使用的 layout。

我们将使用 pipeline barrier 将 image layout 从 `VK_IMAGE_LAYOUT_UNDEFINED` 转换为 `VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL`：

```cpp
void transition_image_layout(
    uint32_t imageIndex,
    vk::ImageLayout oldLayout,
    vk::ImageLayout newLayout,
    vk::AccessFlags2 srcAccessMask,
    vk::AccessFlags2 dstAccessMask,
    vk::PipelineStageFlags2 srcStageMask,
    vk::PipelineStageFlags2 dstStageMask
) {
    vk::ImageMemoryBarrier2 barrier = {
        .srcStageMask = srcStageMask,
        .srcAccessMask = srcAccessMask,
        .dstStageMask = dstStageMask,
        .dstAccessMask = dstAccessMask,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = swapChainImages[imageIndex],
        .subresourceRange = {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };
    vk::DependencyInfo dependencyInfo = {
        .dependencyFlags = {},
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier
    };
    commandBuffer.pipelineBarrier2(dependencyInfo);
}
```

此函数将用于在渲染前后进行 image layout 转换。

## 启动 Dynamic Rendering

使用 dynamic rendering，我们不需要创建 render pass 或 framebuffer。相反，我们在开始渲染时直接指定 attachment：

```cpp
// 在开始渲染之前，将 swap chain image 转换为 COLOR_ATTACHMENT_OPTIMAL
transition_image_layout(
    imageIndex,
    vk::ImageLayout::eUndefined,
    vk::ImageLayout::eColorAttachmentOptimal,
    {},                                                         // srcAccessMask（无需等待之前的操作）
    vk::AccessFlagBits2::eColorAttachmentWrite,                 // dstAccessMask
    vk::PipelineStageFlagBits2::eColorAttachmentOutput,         // srcStage
    vk::PipelineStageFlagBits2::eColorAttachmentOutput          // dstStage
);
```

首先，我们将 image layout 转换为 `VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL`。然后，我们设置 color attachment：

```cpp
vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
vk::RenderingAttachmentInfo attachmentInfo = {
    .imageView = swapChainImageViews[imageIndex],
    .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
    .loadOp = vk::AttachmentLoadOp::eClear,
    .storeOp = vk::AttachmentStoreOp::eStore,
    .clearValue = clearColor
};
```

`imageView` 参数指定要渲染到哪个 image view。`imageLayout` 参数指定渲染过程中 image 所处的 layout。`loadOp` 参数指定渲染前对 image 做什么，`storeOp` 参数指定渲染后对 image 做什么。我们使用 `VK_ATTACHMENT_LOAD_OP_CLEAR` 在渲染前将 image 清除为黑色，并使用 `VK_ATTACHMENT_STORE_OP_STORE` 存储渲染后的 image 以供后续使用。

接下来，我们设置 rendering info：

```cpp
vk::RenderingInfo renderingInfo = {
    .renderArea = { .offset = { 0, 0 }, .extent = swapChainExtent },
    .layerCount = 1,
    .colorAttachmentCount = 1,
    .pColorAttachments = &attachmentInfo
};
```

`renderArea` 参数定义渲染区域的大小，类似于 render pass 中的渲染区域。`layerCount` 参数指定要渲染的层数，对于非分层 image 为 1。`colorAttachmentCount` 和 `pColorAttachments` 参数指定要渲染到的 color attachment。

现在我们可以开始渲染了：

```cpp
commandBuffer.beginRendering(renderingInfo);
```

所有录制命令的函数都可以通过其 `vkCmd` 前缀来识别。它们都返回 `void`，因此在录制完成之前不会有错误处理。

`beginRendering` 命令的参数是我们刚刚设置的 rendering info，它指定了要渲染到的 attachment 和渲染区域。

## 基本绘制命令

我们现在可以绑定 graphics pipeline：

```cpp
commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline);
```

第一个参数指定 pipeline 对象是 graphics pipeline 还是 compute pipeline。我们现在已经告诉 Vulkan 在 graphics pipeline 中执行哪些操作，以及在 fragment shader 中使用哪个 attachment。

如在 fixed functions 章节中所述，我们将此 pipeline 的 viewport 和 scissor 状态指定为动态的。因此，我们需要在发出绘制命令之前在 command buffer 中设置它们：

```cpp
commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height), 0.0f, 1.0f));
commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));
```

现在我们准备好发出绘制三角形的命令了：

```cpp
commandBuffer.draw(3, 1, 0, 0);
```

实际的 `vkCmdDraw` 函数有些平淡无奇，但它之所以如此简单，是因为我们提前指定了所有信息。除 command buffer 外，它还有以下参数：

- `vertexCount`：即使我们没有 vertex buffer，我们仍然有 3 个顶点要绘制。
- `instanceCount`：用于实例化渲染，如果不使用实例化渲染则使用 `1`。
- `firstVertex`：用作 vertex buffer 的偏移量，定义 `SV_VertexId` 的最小值。
- `firstInstance`：用作实例化渲染的偏移量，定义 `SV_InstanceID` 的最小值。

## 收尾工作

渲染现在可以结束了：

```cpp
commandBuffer.endRendering();
```

渲染结束后，我们需要将 image layout 转换回 `VK_IMAGE_LAYOUT_PRESENT_SRC_KHR`，以便可以将其呈现到屏幕：

```cpp
// 渲染结束后，将 swap chain image 转换为 PRESENT_SRC
transition_image_layout(
    imageIndex,
    vk::ImageLayout::eColorAttachmentOptimal,
    vk::ImageLayout::ePresentSrcKHR,
    vk::AccessFlagBits2::eColorAttachmentWrite,             // srcAccessMask
    {},                                                     // dstAccessMask
    vk::PipelineStageFlagBits2::eColorAttachmentOutput,     // srcStage
    vk::PipelineStageFlagBits2::eBottomOfPipe               // dstStage
);
```

我们已经完成了 command buffer 的录制：

```cpp
commandBuffer.end();
```

在下一章中，我们将编写主循环的代码，它将从 swap chain 获取一个 image，录制并执行 command buffer，然后将完成的 image 返回给 swap chain。
