# 多帧并行（Frames in flight）

> 原文：[https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/03_Drawing/03_Frames_in_flight.html](https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/03_Drawing/03_Frames_in_flight.html)

我们的渲染循环目前有一个明显的缺陷。我们需要等待上一帧完成才能开始渲染下一帧，这会导致主机不必要的空闲。

解决方法是允许多个帧同时处于"进行中"（in-flight）状态，也就是说，允许一帧的渲染不干扰下一帧的录制。我们如何做到这一点？渲染过程中访问和修改的任何资源都必须被复制。因此，我们需要多个 command buffer、semaphore 和 fence。在后续章节中，我们还将添加其他资源的多个实例，所以我们会再次看到这个概念。

首先，在程序顶部添加一个常量，定义应该同时处理多少帧：

```cpp
constexpr int MAX_FRAMES_IN_FLIGHT = 2;
```

我们选择数字 2，是因为不希望 CPU 相对于 GPU 走得太远。有了两个 frame in flight，CPU 和 GPU 可以同时处理各自的任务。如果 CPU 提前完成，它将等待 GPU 完成渲染后再提交更多工作。有了三个或更多 frame in flight，CPU 可能领先于 GPU，增加帧延迟。通常，额外的延迟是不可取的。但让应用程序控制 frame in flight 的数量，是 Vulkan 明确性的又一体现。

每一帧都应该有自己的 command buffer、一组 semaphore 和 fence。重命名并将它们改为对象的 `std::vector`：

```cpp
std::vector<vk::raii::CommandBuffer> commandBuffers;

...

std::vector<vk::raii::Semaphore> presentCompleteSemaphores;
std::vector<vk::raii::Semaphore> renderFinishedSemaphores;
std::vector<vk::raii::Fence> inFlightFences;
```

然后我们需要创建多个 command buffer。将 `createCommandBuffer` 重命名为 `createCommandBuffers`。接下来我们需要将 command buffer 向量的大小调整为 `MAX_FRAMES_IN_FLIGHT`，修改 `VkCommandBufferAllocateInfo` 以包含那么多 command buffer，然后将目标改为我们的 command buffer 向量：

```cpp
void createCommandBuffers() {
    commandBuffers.clear();
    vk::CommandBufferAllocateInfo allocInfo{ .commandPool = commandPool, .level = vk::CommandBufferLevel::ePrimary,
                                           .commandBufferCount = MAX_FRAMES_IN_FLIGHT };
    commandBuffers = vk::raii::CommandBuffers( device, allocInfo );
}
```

`createSyncObjects` 函数应修改为创建所有对象：

```cpp
void createSyncObjects() {
		assert(presentCompleteSemaphores.empty() && renderFinishedSemaphores.empty() && inFlightFences.empty());

		for (size_t i = 0; i < swapChainImages.size(); i++)
		{
			renderFinishedSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
		}

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			presentCompleteSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
			inFlightFences.emplace_back(device, vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled});
		}
}
```

为了每帧都使用正确的对象，我们需要跟踪当前帧。我们将为此使用一个帧索引：

```cpp
uint32_t frameIndex = 0;
```

现在可以修改 `drawFrame` 函数以使用正确的对象：

```cpp
void drawFrame() {
		auto fenceResult = device.waitForFences(*inFlightFences[frameIndex], vk::True, UINT64_MAX);
		if (fenceResult != vk::Result::eSuccess)
		{
			throw std::runtime_error("failed to wait for fence!");
		}
		device.resetFences(*inFlightFences[frameIndex]);

		auto [result, imageIndex] = swapChain.acquireNextImage(UINT64_MAX, *presentCompleteSemaphores[frameIndex], nullptr);

		commandBuffers[frameIndex].reset();
		recordCommandBuffer(imageIndex);

		vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
		const vk::SubmitInfo   submitInfo{.waitSemaphoreCount   = 1,
		                                  .pWaitSemaphores      = &*presentCompleteSemaphores[frameIndex],
		                                  .pWaitDstStageMask    = &waitDestinationStageMask,
		                                  .commandBufferCount   = 1,
		                                  .pCommandBuffers      = &*commandBuffers[frameIndex],
		                                  .signalSemaphoreCount = 1,
		                                  .pSignalSemaphores    = &*renderFinishedSemaphores[imageIndex]};
		queue.submit(submitInfo, *inFlightFences[frameIndex]);
}
```

当然，我们不应忘记每次都推进到下一帧：

```cpp
void drawFrame() {
    ...

    frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
}
```

通过使用取模（%）运算符，我们确保帧索引在每 `MAX_FRAMES_IN_FLIGHT` 帧入队后循环回来。

我们现在已经实现了所有必要的同步，以确保入队的工作帧数不超过 `MAX_FRAMES_IN_FLIGHT`，并且这些帧不会相互干扰。注意，对于代码的其他部分，例如最终的清理工作，依赖更粗粒度的同步（如 `vkDeviceWaitIdle`）是可以的。你应该根据性能需求来决定使用哪种方法。

此外，我们可以使用 timeline semaphore 来替代这里介绍的二进制 semaphore。要查看如何使用 timeline semaphore 的示例，请参阅 [compute shader 章节](../../11_Compute_Shader.html)。注意，timeline semaphore 对于像该示例中那样同时处理 compute 队列和 graphics 队列的情况特别有用。这种简单二进制 semaphore 的方法可以被认为是更传统的同步方式。

要通过示例进一步了解同步，请参阅 Khronos 提供的[这份详尽概述](https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples#swapchain-image-acquire-and-present)。

在[下一章](../04_Swap_chain_recreation.html)中，我们将处理一个性能良好的 Vulkan 程序所需的最后一件小事。
