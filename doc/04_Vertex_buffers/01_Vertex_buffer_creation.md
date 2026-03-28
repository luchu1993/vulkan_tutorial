# Vertex Buffer 的创建（Vertex Buffer Creation）

> 原文：[https://docs.vulkan.org/tutorial/latest/04_Vertex_buffers/01_Vertex_buffer_creation.html](https://docs.vulkan.org/tutorial/latest/04_Vertex_buffers/01_Vertex_buffer_creation.html)

## 介绍（Introduction）

Vulkan 中的 buffer 是内存中用于存储任意数据的区域，可供显卡读取。它们可以用于存储顶点数据，也可以用于其他用途。与之前接触过的 Vulkan 对象不同，buffer 不会自动分配内存——程序员必须手动管理内存分配。

## Buffer 创建（Buffer Creation）

创建一个新函数 `createVertexBuffer`，并在 `initVulkan` 中于 `createCommandBuffers` 之前调用它：

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
    createVertexBuffer();
    createCommandBuffers();
    createSyncObjects();
}

void createVertexBuffer() {

}
```

填写一个 `VkBufferCreateInfo` struct：

```cpp
vk::BufferCreateInfo bufferInfo{ .size = sizeof(vertices[0]) * vertices.size(), .usage = vk::BufferUsageFlagBits::eVertexBuffer, .sharingMode = vk::SharingMode::eExclusive };
```

`size` 字段以字节为单位指定 buffer 的大小。`usage` 字段指明 buffer 数据的用途。`sharingMode` 可以设置为独占模式（exclusive）或在多个 queue family 之间共享。定义一个类成员变量并创建 buffer：

```cpp
vk::raii::Buffer vertexBuffer = nullptr;

void createVertexBuffer() {
    vk::BufferCreateInfo bufferInfo{ .size = sizeof(vertices[0]) * vertices.size(), .usage = vk::BufferUsageFlagBits::eVertexBuffer, .sharingMode = vk::SharingMode::eExclusive };
    vertexBuffer = vk::raii::Buffer(device, bufferInfo);
}
```

## 内存需求（Memory Requirements）

使用 `vkGetBufferMemoryRequirements` 查询内存需求：

```cpp
vk::MemoryRequirements memRequirements = vertexBuffer.getMemoryRequirements();
```

`VkMemoryRequirements` struct 包含以下字段：

- `size`：所需内存的字节大小（可能与请求的大小不同）
- `alignment`：在已分配区域中所需的偏移对齐量
- `memoryTypeBits`：适合该 buffer 的内存类型的位域

创建一个 `findMemoryType` 辅助函数来查找合适的内存类型：

```cpp
uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) {

}
```

查询可用的内存类型：

```cpp
vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();

for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
        return i;
    }
}

throw std::runtime_error("failed to find suitable memory type!");
```

## 内存分配（Memory Allocation）

使用 `VkMemoryAllocateInfo` 分配内存：

```cpp
vk::MemoryAllocateInfo memoryAllocateInfo{
    .allocationSize = memRequirements.size,
    .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
};
```

创建一个类成员变量并分配内存：

```cpp
vk::raii::Buffer vertexBuffer = nullptr;
vk::raii::DeviceMemory vertexBufferMemory = nullptr;

vertexBufferMemory = vk::raii::DeviceMemory( device, memoryAllocateInfo );
```

将 buffer 绑定到内存：

```cpp
vertexBuffer.bindMemory( *vertexBufferMemory, 0 );
```

## 填充 Vertex Buffer（Filling the Vertex Buffer）

将 buffer 内存映射到 CPU 可访问的内存：

```cpp
void* data = vertexBufferMemory.mapMemory(0, bufferInfo.size);
memcpy(data, vertices.data(), bufferInfo.size);
vertexBufferMemory.unmapMemory();
```

使用 `VK_MEMORY_PROPERTY_HOST_COHERENT_BIT` 可以确保映射内存与已分配内存的内容保持一致。"将数据传输到 GPU 是一个在后台发生的操作"，其完成由下一次 `vkQueueSubmit` 调用来保证。

## 绑定 Vertex Buffer（Binding the Vertex Buffer）

在 `recordCommandBuffer` 中扩展代码以绑定 vertex buffer：

```cpp
commandBuffers[frameIndex].bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);

commandBuffers[frameIndex].bindVertexBuffers(0, *vertexBuffer, {0});

commandBuffers[frameIndex].draw(3, 1, 0, 0);
```

`vkCmdBindVertexBuffers` 函数将 vertex buffer 绑定到 binding。将 draw call 更新为使用实际的顶点数量。

示例修改——将顶部顶点的颜色改为白色：

```cpp
const std::vector<Vertex> vertices = {
    {{0.0f, -0.5f}, {1.0f, 1.0f, 1.0f}},
    {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
    {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
};
```

下一章将介绍一种使用 staging buffer 复制顶点数据的更高性能方法。
