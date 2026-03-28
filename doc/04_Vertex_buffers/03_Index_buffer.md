# Index Buffer

> 原文：[https://docs.vulkan.org/tutorial/latest/04_Vertex_buffers/03_Index_buffer.html](https://docs.vulkan.org/tutorial/latest/04_Vertex_buffers/03_Index_buffer.html)

## 介绍（Introduction）

在实际应用程序中渲染的 3D 网格，通常会在多个三角形之间共享顶点。即使是绘制一个矩形这样简单的图形，也需要两个三角形，这意味着 vertex buffer 需要六个顶点，其中两个顶点的数据是重复的，产生了 50% 的冗余。而对于复杂网格来说，顶点复用的情况更为严重，平均每个顶点被三个三角形共用。

index buffer 通过充当"指向 vertex buffer 的指针数组"来解决这个问题。它使得我们可以对顶点数据进行重新排序，并对多个顶点复用已有的数据。对于一个拥有四个唯一顶点的矩形，前三个索引定义右上角的三角形，后三个索引定义左下角的三角形。

## Index Buffer 的创建（Index Buffer Creation）

修改顶点数据，使其表示矩形的四个角：

```cpp
const std::vector<Vertex> vertices = {
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
    {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
};
```

添加一个 `indices` 数组来表示 index buffer 的内容：

```cpp
const std::vector<uint16_t> indices = {
    0, 1, 2, 2, 3, 0
};
```

根据顶点数量的不同，index buffer 可以使用 `uint16_t` 或 `uint32_t`。当唯一顶点数少于 65535 时，`uint16_t` 即可胜任。

通过定义类成员变量，将索引数据上传到 `VkBuffer`：

```cpp
vk::raii::Buffer vertexBuffer = nullptr;
vk::raii::DeviceMemory vertexBufferMemory = nullptr;
vk::raii::Buffer indexBuffer = nullptr;
vk::raii::DeviceMemory indexBufferMemory = nullptr;
```

`createIndexBuffer` 函数与 `createVertexBuffer` 几乎完全相同：

```cpp
void initVulkan() {
    ...
    createVertexBuffer();
    createIndexBuffer();
    ...
}

void createIndexBuffer() {
    vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    vk::raii::Buffer stagingBuffer({});
    vk::raii::DeviceMemory stagingBufferMemory({});
    createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

    void* data = stagingBufferMemory.mapMemory(0, bufferSize);
    memcpy(data, indices.data(), (size_t) bufferSize);
    stagingBufferMemory.unmapMemory();

    createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal, indexBuffer, indexBufferMemory);

    copyBuffer(stagingBuffer, indexBuffer, bufferSize);
}
```

`bufferSize` 等于索引数量乘以索引类型的大小。`indexBuffer` 的 usage 应为 `VK_BUFFER_USAGE_INDEX_BUFFER_BIT` 而非 `VK_BUFFER_USAGE_VERTEX_BUFFER_BIT`。整个过程是：创建一个 staging buffer，将索引内容复制进去，然后再传输到最终的 device local index buffer。

## 使用 Index Buffer（Using an Index Buffer）

在 `recordCommandBuffer` 中使用 index buffer 进行绘制需要做两处修改。首先，像绑定 vertex buffer 一样绑定 index buffer。注意，只能有一个 index buffer 处于激活状态；对不同的 vertex attribute 分别使用不同索引是不可能的。

```cpp
commandBuffers[frameIndex].bindVertexBuffers(0, *vertexBuffer, {0});
commandBuffers[frameIndex].bindIndexBuffer( *indexBuffer, 0, vk::IndexType::eUint16 );
```

使用 `vkCmdBindIndexBuffer` 绑定 index buffer，需要指定 index buffer、字节偏移量以及索引数据类型。可选的类型有 `VK_INDEX_TYPE_UINT16` 和 `VK_INDEX_TYPE_UINT32`。

将 `vkCmdDraw` 替换为 `vkCmdDrawIndexed`：

```cpp
commandBuffers[frameIndex].drawIndexed(indices.size(), 1, 0, 0, 0);
```

该函数的前两个参数分别指定索引数量和 instance 数量。由于我们不使用 instancing，指定 `1` 个 instance。索引数量代表传递给 vertex shader 的顶点数。下一个参数指定 index buffer 的偏移量；值为 `1` 则从第二个索引开始读取。倒数第二个参数指定一个偏移量，在用于索引 vertex buffer 之前加到顶点索引上。最后一个参数指定 instancing 的偏移量，此处不使用。

驱动程序开发者建议将多个 buffer 资源的内存从单次分配中获取，并将 vertex buffer 和 index buffer 等多个 buffer 存储到同一个 `VkBuffer` 中，通过命令中的偏移量来区分。这样数据在内存中更为紧凑，有助于提升缓存友好性。如果在相同的渲染操作期间某块内存没有被使用，甚至可以将同一块内存复用给多个资源，前提是数据需要被刷新。这种做法被称为 _aliasing_（内存别名），部分 Vulkan 函数为此提供了专门的标志。
