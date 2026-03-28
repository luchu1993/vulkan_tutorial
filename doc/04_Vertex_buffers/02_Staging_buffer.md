# Staging Buffer

> 原文：[https://docs.vulkan.org/tutorial/latest/04_Vertex_buffers/02_Staging_buffer.html](https://docs.vulkan.org/tutorial/latest/04_Vertex_buffers/02_Staging_buffer.html)

## 介绍（Introduction）

我们目前拥有的 vertex buffer 可以正常工作，但允许我们从 CPU 访问它的内存类型，对于显卡本身的读取来说可能并不是最优的内存类型。最优的内存带有 `VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT` 标志，在专用显卡上通常无法由 CPU 访问。在本章中，我们将创建两个 vertex buffer。一个是位于 CPU 可访问内存中的 _staging buffer_，用于从顶点数组向其上传数据；另一个是位于 device local 内存中的最终 vertex buffer。然后我们将使用 buffer copy 命令将数据从 staging buffer 移动到实际的 vertex buffer。

## Transfer Queue

buffer copy 命令需要支持 transfer 操作的 queue family，这通过 `VK_QUEUE_TRANSFER_BIT` 来标识。好消息是，任何具备 `VK_QUEUE_GRAPHICS_BIT` 或 `VK_QUEUE_COMPUTE_BIT` 能力的 queue family 都已经隐式地支持 `VK_QUEUE_TRANSFER_BIT` 操作，实现无需在这些情况下在 `queueFlags` 中明确列出它。

如果你喜欢挑战，你仍然可以尝试专门使用一个不同的 queue family 来执行 transfer 操作。这需要你对程序做出以下修改：

- 修改 `QueueFamilyIndices` 和 `findQueueFamilies`，以显式查找带有 `VK_QUEUE_TRANSFER_BIT` 位但没有 `VK_QUEUE_GRAPHICS_BIT` 的 queue family。
- 修改 `createLogicalDevice`，以请求 transfer queue 的句柄。
- 为提交到 transfer queue family 的 command buffer 创建第二个 command pool。
- 将资源的 `sharingMode` 改为 `VK_SHARING_MODE_CONCURRENT`，并同时指定 graphics 和 transfer queue family。
- 将所有 transfer 命令（如 `vkCmdCopyBuffer`，本章会用到）提交到 transfer queue，而非 graphics queue。

这需要一些工作，但会让你深入了解资源如何在 queue family 之间共享。

## 抽象 Buffer 创建（Abstracting Buffer Creation）

由于我们在本章中会创建多个 buffer，将 buffer 创建代码移到一个辅助函数中是个好主意。创建一个新函数 `createBuffer`，并将 `createVertexBuffer` 中的代码（除映射部分外）移入其中。

```cpp
void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Buffer& buffer, vk::raii::DeviceMemory& bufferMemory) {
    vk::BufferCreateInfo bufferInfo{ .size = size, .usage = usage, .sharingMode = vk::SharingMode::eExclusive };
    buffer = vk::raii::Buffer(device, bufferInfo);
    vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();
    vk::MemoryAllocateInfo allocInfo{ .allocationSize = memRequirements.size, .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties) };
    bufferMemory = vk::raii::DeviceMemory(device, allocInfo);
    buffer.bindMemory(*bufferMemory, 0);
}
```

确保为 buffer 大小、内存属性和用途添加参数，以便我们可以使用该函数创建许多不同类型的 buffer。最后两个参数是用于写入句柄的输出变量。

现在你可以从 `createVertexBuffer` 中删除 buffer 创建和内存分配代码，直接调用 `createBuffer`：

```cpp
void createVertexBuffer() {
    vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
    createBuffer(bufferSize, vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, vertexBuffer, vertexBufferMemory);
    void* data = vertexBufferMemory.mapMemory(0, bufferSize);
    memcpy(data, vertices.data(), (size_t) bufferSize);
    vertexBufferMemory.unmapMemory();
}
```

运行程序，确认 vertex buffer 仍然正常工作。

## 使用 Staging Buffer（Using a Staging Buffer）

现在我们要修改 `createVertexBuffer`，使其仅将 host visible buffer 用作临时 buffer，并使用 device local buffer 作为实际的 vertex buffer。

```cpp
void createVertexBuffer() {
    vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    vk::BufferCreateInfo stagingInfo{ .size = bufferSize, .usage = vk::BufferUsageFlagBits::eTransferSrc, .sharingMode = vk::SharingMode::eExclusive };
    vk::raii::Buffer stagingBuffer(device, stagingInfo);
    vk::MemoryRequirements memRequirementsStaging = stagingBuffer.getMemoryRequirements();
    vk::MemoryAllocateInfo memoryAllocateInfoStaging{  .allocationSize = memRequirementsStaging.size, .memoryTypeIndex = findMemoryType(memRequirementsStaging.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent) };
    vk::raii::DeviceMemory stagingBufferMemory(device, memoryAllocateInfoStaging);

    stagingBuffer.bindMemory(stagingBufferMemory, 0);
    void* dataStaging = stagingBufferMemory.mapMemory(0, stagingInfo.size);
    memcpy(dataStaging, vertices.data(), stagingInfo.size);
    stagingBufferMemory.unmapMemory();

    vk::BufferCreateInfo bufferInfo{ .size = bufferSize,  .usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst, .sharingMode = vk::SharingMode::eExclusive };
    vertexBuffer = vk::raii::Buffer(device, bufferInfo);

    vk::MemoryRequirements memRequirements = vertexBuffer.getMemoryRequirements();
    vk::MemoryAllocateInfo memoryAllocateInfo{  .allocationSize = memRequirements.size, .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal) };
    vertexBufferMemory = vk::raii::DeviceMemory( device, memoryAllocateInfo );

    vertexBuffer.bindMemory( *vertexBufferMemory, 0 );

    copyBuffer(stagingBuffer, vertexBuffer, stagingInfo.size);
}
```

我们现在使用新的 `stagingBuffer` 及其 `stagingBufferMemory` 来映射和复制顶点数据。在本章中，我们将用到两个新的 buffer usage flag。注意，我们必须创建一个指向新的 `vk::raii::Buffer` 对象的临时指针，因为 `vk::raii::Buffer` 的构造函数被删除，因此无法与 `std::make_unique` 配合使用，这只是一个让其正常工作的技巧。

- `VK_BUFFER_USAGE_TRANSFER_SRC_BIT`：buffer 可用作内存 transfer 操作的源。
- `VK_BUFFER_USAGE_TRANSFER_DST_BIT`：buffer 可用作内存 transfer 操作的目标。

`vertexBuffer` 现在从 device local 的内存类型中分配，这通常意味着我们无法使用 `vkMapMemory`。然而，我们可以将数据从 `stagingBuffer` 复制到 `vertexBuffer`。我们必须通过为 `stagingBuffer` 指定 transfer source 标志、为 `vertexBuffer` 指定 transfer destination 标志（以及 vertex buffer usage 标志）来表明我们打算这样做。

现在我们要编写一个函数，将内容从一个 buffer 复制到另一个 buffer，称为 `copyBuffer`。

```cpp
void copyBuffer(vk::raii::Buffer & srcBuffer, vk::raii::Buffer & dstBuffer, vk::DeviceSize size) {

}
```

内存 transfer 操作通过 command buffer 来执行，就像 draw 命令一样。因此我们必须首先分配一个临时 command buffer。你可能希望为这类短暂存在的 buffer 单独创建一个 command pool，因为实现可能能够应用内存分配优化。在这种情况下，你应该在 command pool 创建时使用 `VK_COMMAND_POOL_CREATE_TRANSIENT_BIT` 标志。

```cpp
void copyBuffer(vk::raii::Buffer & srcBuffer, vk::raii::Buffer & dstBuffer, vk::DeviceSize size) {
    vk::CommandBufferAllocateInfo allocInfo{ .commandPool = commandPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1 };
    vk::raii::CommandBuffer commandCopyBuffer = std::move(device.allocateCommandBuffers(allocInfo).front());
}
```

然后立即开始录制 command buffer：

```cpp
commandCopyBuffer.begin(vk::CommandBufferBeginInfo { .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
```

我们只会使用该 command buffer 一次，并在 copy 操作执行完毕后再从函数返回。使用 `VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT` 告知驱动程序我们的意图，这是良好实践。

```cpp
commandCopyBuffer.copyBuffer(srcBuffer, dstBuffer, vk::BufferCopy(0, 0, size));
```

buffer 内容通过 `vkCmdCopyBuffer` 命令来传输。它接受源 buffer 和目标 buffer 作为参数，以及一个要复制的区域数组。这些区域由 `VkBufferCopy` struct 定义，包含源 buffer 偏移量、目标 buffer 偏移量和大小。与 `vkMapMemory` 命令不同，这里不能指定 `VK_WHOLE_SIZE`。

```cpp
commandCopyBuffer.end();
```

该 command buffer 只包含 copy 命令，所以我们可以在此之后立即停止录制。现在执行 command buffer 以完成 transfer：

```cpp
graphicsQueue.submit(vk::SubmitInfo{ .commandBufferCount = 1, .pCommandBuffers = &*commandCopyBuffer }, nullptr);
graphicsQueue.waitIdle();
```

与 draw 命令不同，这次我们不需要等待任何事件。我们只想立即在 buffer 上执行 transfer。同样，有两种方式可以等待 transfer 完成：可以使用 fence 并通过 `vkWaitForFences` 等待，或者直接等待 transfer queue 变为空闲状态，使用 `vkQueueWaitIdle`。使用 fence 可以让你同时调度多个 transfer 并等待它们全部完成，而不是一次执行一个。这可以给驱动程序更多的优化机会。

现在我们可以从 `createVertexBuffer` 函数中调用 `copyBuffer`，将顶点数据移动到 device local buffer：

```cpp
vk::BufferCreateInfo bufferInfo{ .size = bufferSize,  .usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst, .sharingMode = vk::SharingMode::eExclusive };
vertexBuffer = vk::raii::Buffer(device, bufferInfo);

vk::MemoryRequirements memRequirements = vertexBuffer.getMemoryRequirements();
vk::MemoryAllocateInfo memoryAllocateInfo{  .allocationSize = memRequirements.size, .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal) };
vertexBufferMemory = vk::raii::DeviceMemory( device, memoryAllocateInfo );

vertexBuffer.bindMemory( *vertexBufferMemory, 0 );

copyBuffer(stagingBuffer, vertexBuffer, bufferSize);
```

将数据从 staging buffer 复制到 device buffer 后，RAII buffer 对象将自动清理自身并释放内存。

运行程序，验证你是否仍然看到熟悉的三角形。此时改进可能并不明显，但其顶点数据现在已经从高性能内存中加载。当我们开始渲染更复杂的几何体时，这将变得很重要。

## 结论（Conclusion）

需要指出的是，在实际应用程序中，你不应该为每个单独的 buffer 调用 `vkAllocateMemory`。同时进行的内存分配最大数量受到 `maxMemoryAllocationCount` 物理设备限制的约束，即使在 NVIDIA GTX 1080 这样的高端硬件上，该值也可能低至 `4096`。为大量对象同时分配内存的正确方式是创建一个自定义分配器，通过使用我们在许多函数中看到的 `offset` 参数，将单次分配拆分给许多不同的对象使用。

你可以自己实现这样的分配器，或者使用 GPUOpen 计划提供的 [VulkanMemoryAllocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator) 库。然而，对于本教程而言，为每个资源单独分配内存是可以接受的，因为我们目前远未达到这些限制。

在[下一章](03_Index_buffer.html)中，我们将学习用于顶点复用的 index buffer。
