# 渲染与呈现（Rendering and presentation）

> 原文：[https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/03_Drawing/02_Rendering_and_presentation.html](https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/03_Drawing/02_Rendering_and_presentation.html)

这一章是所有内容汇聚在一起的地方。我们将编写 `drawFrame` 函数，该函数将从主循环中调用，以将三角形显示到屏幕上。让我们先创建这个函数并从 `mainLoop` 中调用它：

```cpp
void mainLoop() {
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        drawFrame();
    }
}

...

void drawFrame() {

}
```

## 一帧的概述（Outline of a frame）

从高层次来看，在 Vulkan 中渲染一帧包含一组通用步骤：

- 等待上一帧完成
- 从 swap chain 获取一个 image
- 录制一个 command buffer，将场景绘制到该 image 上
- 提交录制好的 command buffer
- 呈现 swap chain image

虽然我们将在后续章节中扩展绘制函数，但现在这是我们渲染循环的核心。

## 同步（Synchronization）

Vulkan 的核心设计理念之一是 GPU 上的执行同步是显式的。操作顺序由我们使用各种同步原语来定义，这些原语告诉驱动程序我们希望事情运行的顺序。这意味着许多在 GPU 上开始执行工作的 Vulkan API 调用是异步的，函数会在操作完成之前返回。

在本章中，有许多事件需要我们显式排序，因为它们发生在 GPU 上，例如：

- 从 swap chain 获取一个 image
- 执行将内容绘制到已获取 image 上的命令
- 将该 image 呈现到屏幕以供展示，然后将其返回给 swap chain

这些事件中的每一个都通过单个函数调用来启动，但都是异步执行的。函数调用会在操作实际完成之前返回，执行顺序也是未定义的。这很不幸，因为每个操作都依赖于前一个操作完成。因此，我们需要探索可以使用哪些原语来实现所需的排序。

### Semaphore

二进制 semaphore 用于在队列操作之间添加顺序。队列操作是指我们提交到队列的工作，无论是在 command buffer 中还是在函数内部，我们将在后面看到。队列的例子包括 graphics 队列和 presentation 队列。Semaphore 既用于对同一队列内的工作排序，也用于在不同队列之间排序。

Vulkan 中恰好有两种 semaphore：二进制和时间线（timeline）。由于本教程中只使用二进制 semaphore，我们将不讨论 timeline semaphore。之后提到 semaphore 这个术语时，专指二进制 semaphore。

二进制 semaphore 要么处于未发信号（unsignaled）状态，要么处于已发信号（signaled）状态。它的初始状态是未发信号。我们使用二进制 semaphore 对队列操作排序的方式是：在一个队列操作中将同一个 semaphore 作为"信号"（signal）semaphore 提供，在另一个队列操作中将其作为"等待"（wait）semaphore 提供。例如，假设我们有 semaphore S 以及我们想按顺序执行的队列操作 A 和 B。我们告诉 Vulkan 的是，操作 A 在完成执行时将"发信号"给 semaphore S，而操作 B 在开始执行前将"等待" semaphore S。当操作 A 完成时，semaphore S 将被发信号，而操作 B 在 S 被发信号之前不会开始。操作 B 开始执行后，semaphore S 自动重置回未发信号状态，以便可以再次使用。

刚才描述内容的伪代码：

```cpp
VkCommandBuffer A, B = ... // 录制 command buffer
VkSemaphore S = ... // 创建一个 semaphore

// 将 A 入队，完成时发信号给 S——立即开始执行
vkQueueSubmit(work: A, signal: S, wait: None)

// 将 B 入队，等待 S 才开始
vkQueueSubmit(work: B, signal: None, wait: S)
```

注意，在这段代码中，两次对 `vkQueueSubmit()` 的调用都立即返回——等待只发生在 GPU 上。CPU 继续运行而不阻塞。要让 CPU 等待，我们需要一种不同的同步原语，我们接下来将介绍它。

### Fence

Fence 有类似的用途，它也用于同步执行，但用于在 CPU（即主机）上排序执行。具体来说，如果主机需要知道 GPU 何时完成了某些工作，我们就使用 fence。

与 semaphore 类似，fence 要么处于已发信号状态，要么处于未发信号状态。每当我们提交工作执行时，我们可以将一个 fence 附加到该工作上。当工作完成时，fence 将被发信号。然后我们可以让主机等待 fence 被发信号，从而保证工作在主机继续之前已经完成。

一个具体的例子是截图。假设我们已经在 GPU 上完成了必要的工作。现在需要将 image 从 GPU 传输到主机，然后将内存保存到文件。我们有 command buffer A 执行传输和 fence F。我们提交带有 fence F 的 command buffer A，然后立即告诉主机等待 F 发信号。这会导致主机阻塞，直到 command buffer A 完成执行。因此，我们可以安全地让主机将文件保存到磁盘，因为内存传输已经完成。

刚才描述内容的伪代码：

```cpp
VkCommandBuffer A = ... // 录制包含传输的 command buffer
VkFence F = ... // 创建 fence

// 将 A 入队，立即开始工作，完成时发信号给 F
vkQueueSubmit(work: A, fence: F)

vkWaitForFence(F) // 阻塞执行，直到 A 完成执行

save_screenshot_to_disk() // 不能在传输完成前运行
```

与 semaphore 示例不同，这个示例确实会阻塞主机执行。这意味着主机除了等待执行完成之外不会做任何事情。在这种情况下，我们必须确保传输完成后才能将截图保存到磁盘。

一般来说，除非必要，否则最好不要阻塞主机。我们希望让 GPU 和主机都保持忙碌状态，做有用的工作。等待 fence 发信号不是有用的工作。因此，我们更倾向于使用 semaphore，或其他尚未涉及的同步原语来同步我们的工作。

Fence 必须手动重置才能将其恢复为未发信号状态。这是因为 fence 用于控制主机的执行，因此由主机决定何时重置 fence。相比之下，semaphore 用于在 GPU 上排序工作，无需主机参与。

总结：semaphore 用于指定 GPU 上操作的执行顺序，而 fence 用于保持 CPU 和 GPU 相互同步。

### 如何选择？（What to choose?）

我们有两个同步原语可以使用，恰好有两个地方需要应用同步：swap chain 操作和等待上一帧完成。我们希望对 swap chain 操作使用 semaphore，因为它们发生在 GPU 上，因此我们不希望让主机在能避免的情况下等待。对于等待上一帧完成，我们希望使用 fence，原因正好相反，因为我们需要主机等待。这样做是为了不一次绘制超过一帧。由于我们每帧都要重新录制 command buffer，我们不能在当前帧完成执行之前将下一帧的工作录制到 command buffer 中。我们不希望在 GPU 正在使用 command buffer 时覆盖其当前内容。

## 创建同步对象（Creating the synchronization objects）

我们需要一个 semaphore 来表示已从 swap chain 获取了一个 image 并准备好进行渲染。另一个 semaphore 表示渲染已完成，可以进行呈现，以及一个 fence 来确保每次只渲染一帧。

创建三个类成员来存储这些 semaphore 对象和 fence 对象：

```cpp
vk::raii::Semaphore presentCompleteSemaphore = nullptr;
vk::raii::Semaphore renderFinishedSemaphore = nullptr;
vk::raii::Fence drawFence = nullptr;
```

要创建 semaphore，我们将为本教程这一部分添加最后一个 `create` 函数：`createSyncObjects`：

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
    createSyncObjects();
}

...

void createSyncObjects() {

}
```

创建 semaphore 需要填写 `vk::SemaphoreCreateInfo`，但在当前版本的 API 中，它实际上没有任何与教程相关的字段：

```cpp
void createSyncObjects() {
    presentCompleteSemaphore = vk::raii::Semaphore(device, vk::SemaphoreCreateInfo());
    renderFinishedSemaphore = vk::raii::Semaphore(device, vk::SemaphoreCreateInfo());
    drawFence = vk::raii::Fence(device, {.flags = vk::FenceCreateFlagBits::eSignaled});
}
```

未来版本的 Vulkan API 或扩展可能会为 `flags` 和 `pNext` 参数添加功能，就像对其他结构体所做的那样。

接下来是主要的绘制函数！

## 等待上一帧（Waiting for the previous frame）

在帧的开始，我们希望等待上一帧完成，以便 command buffer 和 semaphore 可以使用。为此，我们调用 `device.waitForFences`：

```cpp
void drawFrame() {
    auto fenceResult = device.waitForFences(*drawFence, vk::True, UINT64_MAX);
}
```

`waitForFences` 函数接受一个 fence 数组，并在主机上等待任意或全部 fence 被发信号后返回。我们在这里传递的 `vk::True` 表示我们希望等待所有 fence，但在只有一个 fence 的情况下这无关紧要。此函数还有一个超时参数，我们将其设置为 64 位无符号整数的最大值 `UINT64_MAX`，这实际上禁用了超时。

接下来，在上一帧完成后从 framebuffer 获取一个 image：

```cpp
void drawFrame() {
    ...
    auto [result, imageIndex] = swapChain.acquireNextImage(UINT64_MAX, *presentCompleteSemaphore, nullptr);
}
```

第一个参数指定等待 image 变为可用的超时时间（纳秒）。使用 64 位无符号整数的最大值意味着我们实际上禁用了超时。

接下来的两个参数指定当 presentation 引擎使用完 image 时要发信号的同步对象。这就是我们可以开始绘制到它的时间点。可以指定 semaphore、fence 或两者。我们在这里使用 `presentCompleteSemaphore` 来实现这个目的。

最后一个参数指定一个变量，用于输出已变为可用的 swap chain image 的索引。该索引指的是我们 `swapChainImages` 数组中的 `VkImage`。我们将使用该索引来选取 `VkFrameBuffer`。然后我们将录制到该 framebuffer 中。

## 录制 Command Buffer

有了指定要使用的 swap chain image 的 imageIndex，我们现在可以录制 command buffer 了。现在调用函数 `recordCommandBuffer` 来录制我们想要的命令。

```cpp
recordCommandBuffer(imageIndex);
```

我们需要确保如果上一帧已经发生，fence 被重置，这样我们之后才知道要等待它。

```cpp
device.resetFences(*drawFence);
```

有了完整录制的 command buffer，我们现在可以提交它了。

## 提交 Command Buffer

队列提交和同步通过 `vk::SubmitInfo` 结构体中的参数配置。

```cpp
vk::PipelineStageFlags waitDestinationStageMask( vk::PipelineStageFlagBits::eColorAttachmentOutput );
const vk::SubmitInfo submitInfo{
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &*presentCompleteSemaphore,
    .pWaitDstStageMask = &waitDestinationStageMask,
    .commandBufferCount = 1,
    .pCommandBuffers = &*commandBuffer,
    .signalSemaphoreCount = 1,
    .pSignalSemaphores = &*renderFinishedSemaphore};
```

前三个参数指定在执行开始之前等待哪些 semaphore，以及在 pipeline 的哪个阶段等待。我们希望等待将颜色写入 image，直到它可用，所以我们指定写入 color attachment 的 graphics pipeline 阶段。这意味着理论上，实现可以在 image 尚不可用时就开始执行我们的 vertex shader 等操作。`waitStages` 数组中的每个条目对应 `pWaitSemaphores` 中具有相同索引的 semaphore。

下一个参数指定实际提交执行的 command buffer。我们简单地提交我们拥有的单个 command buffer。

`pSignalSemaphores` 参数指定 command buffer 完成执行后要发信号的 semaphore。在我们的例子中，我们为此目的使用 `renderFinishedSemaphore`。

```cpp
graphicsQueue.submit(submitInfo, *drawFence);
```

我们现在可以使用 `submit` 将 command buffer 提交到 graphics 队列。该函数以 `vk::SubmitInfo` 结构体数组作为参数，当工作负载更大时可以提高效率。最后一个参数引用一个可选的 fence，该 fence 将在 command buffer 完成执行时被发信号。这让我们知道何时可以安全地重用 command buffer，因此我们要传入 `drawFence`，它将在下一帧中等待。

## Subpass 依赖（Subpass dependencies）

本节是可选内容，比必要的内容更为明确。

请记住，render pass 中的 subpass 会自动处理 image layout 转换。这些转换由 subpass 依赖控制，它们指定 subpass 之间的内存和执行依赖关系。我们现在只有一个 subpass，但在此 subpass 之前和之后的操作也算作隐式的"subpass"。

有两个内置依赖负责处理 render pass 开始和结束时的转换，但前者不会在正确的时间发生。它假设转换发生在 pipeline 的开始，但那时我们还没有获取 image！有两种方法可以解决这个问题。我们可以将 `imageAvailableSemaphore` 的 `waitStages` 更改为 `VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT` 以确保 render pass 在 image 可用之前不会开始，或者我们可以让 render pass 等待 `VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT` 阶段。我在这里选择了第二种方案，因为这是了解 subpass 依赖及其工作原理的好机会。

Subpass 依赖在 `VkSubpassDependency` 结构体中指定。进入 `createRenderPass` 函数并添加一个：

```cpp
vk::SubpassDependency dependency(VK_SUBPASS_EXTERNAL, {});
```

前两个字段指定依赖的索引和依赖的 subpass 的索引。特殊值 `VK_SUBPASS_EXTERNAL` 指的是 render pass 之前或之后的隐式 subpass，具体取决于它是在 `srcSubpass` 还是 `dstSubpass` 中指定。索引 `0` 指我们的 subpass，它是第一个也是唯一一个 subpass。`dstSubpass` 必须始终高于 `srcSubpass`，以防止依赖图中出现循环（除非其中一个 subpass 是 `VK_SUBPASS_EXTERNAL`）。

```cpp
vk::SubpassDependency dependency(VK_SUBPASS_EXTERNAL, {},
        vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eColorAttachmentOutput,
        {}, vk::AccessFlagBits::eColorAttachmentWrite);
```

接下来的两个字段指定要等待的操作以及应该等待这些操作的阶段，处于 color attachment 阶段。后两个字段指定这些操作发生的阶段，涉及 color attachment 的写入。我们需要等待 swap chain 从 image 读取完成后才能访问它。这可以通过等待 color attachment 输出阶段本身来实现。

```cpp
renderPassInfo.dependencyCount = 1;
renderPassInfo.pDependencies = &dependency;
```

`VkRenderPassCreateInfo` 结构体有两个字段用于指定依赖数组。

以上内容完全是可选的，不在[演示代码](../../_attachments/15_hello_triangle.cpp)中重现。

## 呈现（Presentation）

绘制一帧的最后一步是将结果提交回 swap chain，以便最终在屏幕上显示。呈现通过 `drawFrame` 函数末尾的 `vk::PresentInfoKHR` 结构体配置。

```cpp
const vk::PresentInfoKHR presentInfoKHR{
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &*renderFinishedSemaphore,
    .swapchainCount = 1,
    .pSwapchains = &*swapChain,
    .pImageIndices = &imageIndex};
```

前两个参数指定在呈现发生之前等待哪些 semaphore，就像 `vk::SubmitInfo` 一样。由于我们想等待 command buffer 完成执行（即我们的三角形被绘制出来），我们获取将被发信号的 semaphore 并等待它们，因此我们使用 `signalSemaphores`。

接下来的两个参数指定要向其呈现 image 的 swap chain 以及每个 swap chain 的 image 索引。这几乎总是单个的。

```cpp
presentInfo.pResults = nullptr; // 可选
```

还有一个可选参数叫 `pResults`。它允许你指定一个 `vk::Result` 值数组，用于检查每个 swap chain 的呈现是否成功。如果只使用单个 swap chain，则没有必要，因为你可以使用 present 函数的返回值。

```cpp
result = presentQueue.presentKHR(presentInfoKHR);
```

`presentKHR` 函数向 swap chain 提交呈现 image 的请求。我们将在下一章为 `swapChain.acquireNextImage` 和 `queue.presentKHR` 都添加错误处理，因为它们的失败不一定意味着程序应该终止，这与我们迄今为止看到的函数不同。

如果到目前为止你一切都做得正确，那么当你运行程序时，你应该会看到类似以下的内容：

![triangle](https://docs.vulkan.org/tutorial/latest/_images/images/triangle.png)

> 这个彩色三角形看起来可能与你在图形教程中习惯看到的有些不同。这是因为本教程让 shader 在线性颜色空间中进行插值，之后再转换为 sRGB 颜色空间。

太好了！遗憾的是，你会看到当启用 validation layer 时，程序一关闭就会崩溃。从 `debugCallback` 打印到终端的消息告诉我们原因：

![semaphore in use](https://docs.vulkan.org/tutorial/latest/_images/images/semaphore_in_use.png)

请记住，`drawFrame` 中的所有操作都是异步的。这意味着当我们退出 `mainLoop` 中的循环时，绘制和呈现操作可能仍在进行。在那时清理资源是个坏主意。

为了解决这个问题，我们应该在退出 `mainLoop` 并销毁窗口之前，等待逻辑设备完成操作：

```cpp
void mainLoop() {
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        drawFrame();
    }

    device.waitIdle();
}
```

你也可以使用 `vkQueueWaitIdle` 等待特定 command 队列中的操作完成。这些函数可以作为一种非常基本的同步方式来使用。你会看到程序现在在关闭窗口时可以正常退出。

## 结语（Conclusion）

经过了五百多行代码，我们终于到了在屏幕上看到一些东西出现的阶段！引导一个 Vulkan 程序肯定需要大量工作，但关键的收获是 Vulkan 通过其明确性为你提供了大量的控制权。我建议你现在花一些时间重新阅读代码，并在脑海中建立程序中所有 Vulkan 对象的用途以及它们如何相互关联的思维模型。我们将从这里开始，在这些知识的基础上扩展程序的功能。

此外，在未来的章节中，我们将讨论 timeline semaphore 和内存屏障（memory barrier），进一步完善我们对 Vulkan 中同步的理解。同步是充分利用 Vulkan 真正力量的最大领域之一，因此相当复杂。虽然第一次理解起来很复杂，但这真的是接下来内容的基础。当你的工具箱中有了更多工具来处理更细微的事情时，真的会变得越来越容易。

[下一章](03_Frames_in_flight.html)将扩展渲染循环以处理多个 frame in flight。

[C++ 代码](../../_attachments/15_hello_triangle.cpp) / [Slang shader](../../_attachments/09_shader_base.slang) / [GLSL Vertex shader](../../_attachments/09_shader_base.vert) / [GLSL Fragment shader](../../_attachments/09_shader_base.frag)
