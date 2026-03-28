# Descriptor Pool 与 Descriptor Set（Descriptor pool and sets）

> 原文：https://docs.vulkan.org/tutorial/latest/05_Uniform_buffers/01_Descriptor_pool_and_sets.html

## 简介（Introduction）

上一章中的 descriptor set layout 描述了可以被绑定的描述符类型。在本章中，我们将为每个 `VkBuffer` 资源创建一个 descriptor set，以将其绑定到 uniform buffer 描述符。

---

## Descriptor Pool

Descriptor set 不能直接创建，必须像 command buffer 一样从 pool 中分配。与 descriptor set 对应的等价物自然被称为 descriptor pool。我们将编写一个新函数 `createDescriptorPool` 来进行设置。

```cpp
void initVulkan() {
    ...
    createUniformBuffers();
    createDescriptorPool();
    ...
}
...
void createDescriptorPool() {
}
```

我们首先需要使用 `VkDescriptorPoolSize` 结构体描述 descriptor set 将包含哪些描述符类型以及各有多少个。

```cpp
vk::DescriptorPoolSize poolSize(vk::DescriptorType::eUniformBuffer, MAX_FRAMES_IN_FLIGHT);
```

我们将为每一帧分配一个这样的描述符。

该 pool size 结构体由主 `VkDescriptorPoolCreateInfo` 引用：

```cpp
vk::DescriptorPoolCreateInfo poolInfo{ .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, .maxSets = MAX_FRAMES_IN_FLIGHT, .poolSizeCount = 1, .pPoolSizes = &poolSize };
```

除了可用的单个描述符的最大数量之外，我们还需要指定可以分配的 descriptor set 的最大数量。

该结构体有一个类似于 command pool 的可选标志，用于决定是否可以释放单个 descriptor set：`VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT`。创建后我们不打算再修改 descriptor set，因此不需要此标志。可以将 `flags` 保留为默认值 0。

```cpp
vk::raii::DescriptorPool descriptorPool = nullptr;
...
descriptorPool = vk::raii::DescriptorPool(device, poolInfo);
```

添加一个新的类成员来存储 descriptor pool 的句柄，并调用 `vkCreateDescriptorPool` 来创建它。

---

## Descriptor Set

现在我们可以分配 descriptor set 本身了。为此添加一个 `createDescriptorSets` 函数：

```cpp
void initVulkan() {
    ...
    createDescriptorPool();
    createDescriptorSets();
    ...
}
...
void createDescriptorSets() {
}
```

descriptor set 的分配通过 `VkDescriptorSetAllocateInfo` 结构体来描述。你需要指定从哪个 descriptor pool 分配、要分配多少个 descriptor set，以及它们所基于的 descriptor set layout：

```cpp
std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *descriptorSetLayout);
vk::DescriptorSetAllocateInfo allocInfo{ .descriptorPool = descriptorPool, .descriptorSetCount = static_cast<uint32_t>(layouts.size()), .pSetLayouts = layouts.data() };
```

在我们的例子中，我们将为每个飞行中的帧创建一个 descriptor set，所有 descriptor set 具有相同的 layout。不幸的是，我们确实需要 layout 的所有副本，因为下一个函数需要一个与 set 数量匹配的数组。

添加一个类成员来保存 descriptor set 的句柄，并用 `vkAllocateDescriptorSets` 分配它们：

```cpp
vk::raii::DescriptorPool descriptorPool = nullptr;
std::vector<vk::raii::DescriptorSet> descriptorSets;
...
descriptorSets.clear();
descriptorSets = device.allocateDescriptorSets(allocInfo);
```

现在 descriptor set 已经分配完毕，但其中的描述符仍需要配置。我们将添加一个循环来填充每个描述符：

```cpp
for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
}
```

引用 buffer 的描述符（如我们的 uniform buffer 描述符）通过 `VkDescriptorBufferInfo` 结构体进行配置。该结构体指定了 buffer 以及其中包含描述符数据的区域。

```cpp
for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vk::DescriptorBufferInfo bufferInfo{ .buffer = uniformBuffers[i], .offset = 0, .range = sizeof(UniformBufferObject) };
}
```

如果你要覆写整个 buffer（就像我们在这里做的那样），也可以使用 `VK_WHOLE_SIZE` 作为 `range` 的值。

描述符的配置使用 `vkUpdateDescriptorSets` 函数更新，该函数以 `VkWriteDescriptorSet` 结构体数组为参数。

```cpp
vk::WriteDescriptorSet descriptorWrite{ .dstSet = descriptorSets[i], .dstBinding = 0, .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = vk::DescriptorType::eUniformBuffer, .pBufferInfo = &bufferInfo };
```

前两个字段指定要更新的 descriptor set 和 binding。我们给 uniform buffer binding 的索引为 0。记住描述符可以是数组，所以我们还需要指定数组中要更新的第一个索引。我们没有使用数组，所以索引简单地为 0。

我们需要再次指定描述符的类型。可以在一次操作中更新数组中从 `dstArrayElement` 开始的多个描述符。`descriptorCount` 字段指定要更新多少个数组元素。最后一个字段引用一个包含 `descriptorCount` 个结构体的数组，用于实际配置描述符。

实际上需要使用三个字段中的哪一个取决于描述符的类型。`pBufferInfo` 字段用于引用 buffer 数据的描述符，`pImageInfo` 用于引用 image 数据的描述符，`pTexelBufferView` 用于引用 buffer view 的描述符。我们的描述符基于 buffer，所以我们使用 `pBufferInfo`。

```cpp
device.updateDescriptorSets(descriptorWrite, {});
```

更新通过 `vkUpdateDescriptorSets` 应用。它接受两种数组作为参数：`VkWriteDescriptorSet` 数组和 `VkCopyDescriptorSet` 数组。后者可以用于相互复制描述符，顾名思义。

---

## 使用 Descriptor Set（Using descriptor sets）

现在我们需要更新 `recordCommandBuffer` 函数，以使用 `vkCmdBindDescriptorSets` 将每帧正确的 descriptor set 实际绑定到 shader 中的描述符。这需要在 `vkCmdDrawIndexed` 调用之前完成：

```cpp
commandBuffers[frameIndex].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, *descriptorSets[frameIndex], nullptr);
commandBuffers[frameIndex].drawIndexed(indices.size(), 1, 0, 0, 0);
```

与 vertex buffer 和 index buffer 不同，descriptor set 并不是 graphics pipeline 独有的。因此，我们需要指定是否要将 descriptor set 绑定到 graphics 或 compute pipeline。下一个参数是描述符所基于的 layout。接下来的三个参数指定第一个 descriptor set 的索引、要绑定的 set 数量以及要绑定的 set 数组。我们稍后会回到这个问题。最后两个参数指定用于 dynamic descriptor 的偏移量数组，我们将在未来的章节中了解这些内容。

如果你现在运行程序，会发现不幸的是什么都看不见。问题在于，由于我们在 projection 矩阵中做的 Y 轴翻转，顶点现在以逆时针顺序绘制，而不是顺时针顺序。这导致背面剔除（backface culling）触发，阻止了任何几何体被绘制。

进入 `createGraphicsPipeline` 函数，修改 `VkPipelineRasterizationStateCreateInfo` 中的 `frontFace` 来纠正这个问题：

```cpp
vk::PipelineRasterizationStateCreateInfo rasterizer({}, vk::False, vk::False, vk::PolygonMode::eFill,
       vk::CullModeFlagBits::eBack, vk::FrontFace::eCounterClockwise, vk::False, 0.0f, 0.0f, 1.0f, 1.0f);
```

再次运行程序，你现在应该能看到以下画面：

![spinning quad](https://docs.vulkan.org/tutorial/latest/_images/images/spinning_quad.png)

矩形已变为正方形，因为 projection 矩阵现在对宽高比进行了校正。`updateUniformBuffer` 处理了屏幕缩放，所以我们不需要在 `recreateSwapChain` 中重新创建 descriptor set。

---

## 对齐要求（Alignment requirements）

迄今为止我们一直忽略的一件事是，C++ 结构体中的数据应该如何与 shader 中的 uniform 定义精确匹配。

直接在两者中使用相同的类型看起来似乎很明显：

```cpp
struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};
```

```slang
struct UniformBuffer {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};
ConstantBuffer<UniformBuffer> ubo;
```

然而，这并不是全部。例如，尝试将结构体和 shader 修改成这样：

```cpp
struct UniformBufferObject {
    glm::vec2 foo;
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};
```

```slang
struct UniformBuffer {
    float2 foo;
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};
ConstantBuffer<UniformBuffer> ubo;
```

重新编译 shader 和程序并运行，你会发现之前辛辛苦苦做出来的彩色正方形消失了！这是因为我们没有考虑到对齐要求（alignment requirements）。

Vulkan 期望结构体中的数据在内存中以特定方式对齐，例如：

- 标量（Scalar）必须按 N 对齐（= 4 字节，给定 32 位浮点数）
- `float2` 必须按 2N 对齐（= 8 字节）
- `float3` 或 `float4` 必须按 4N 对齐（= 16 字节）
- 嵌套结构体必须按其成员的基本对齐方式向上取整到 16 的倍数
- `float4x4` 矩阵必须与 `float4` 具有相同的对齐方式

你可以在规范中找到完整的对齐要求列表。

我们原来只有三个 mat4 字段的 shader 已经满足了对齐要求。每个 mat4 的大小为 4 × 4 × 4 = 64 字节，所以 `model` 的偏移量为 0，`view` 的偏移量为 64，`proj` 的偏移量为 128。这些都是 16 的倍数，所以它能正常工作。

新结构体以一个只有 8 字节大小的 `vec2` 开头，从而打乱了所有偏移量。现在 `model` 的偏移量为 8，`view` 的偏移量为 72，`proj` 的偏移量为 136，没有一个是 16 的倍数。

要解决这个问题，我们可以使用 C++11 中引入的 `alignas` 说明符：

```cpp
struct UniformBufferObject {
    glm::vec2 foo;
    alignas(16) glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};
```

如果你现在重新编译并运行程序，应该会看到 shader 再次正确地接收到了矩阵值。

幸运的是，有一种方法可以在大多数情况下不必考虑这些对齐要求。我们可以在引入 GLM 之前定义 `GLM_FORCE_DEFAULT_ALIGNED_GENTYPES`：

```cpp
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
```

这将强制 GLM 使用已经为我们指定了对齐要求的 `vec2` 和 `mat4` 版本。如果你添加了这个定义，则可以删除 `alignas` 说明符，程序仍然可以正常工作。

不幸的是，如果你开始使用嵌套结构体，这种方法可能会失效。考虑以下 C++ 代码中的定义：

```cpp
struct Foo {
    glm::vec2 v;
};

struct UniformBufferObject {
    Foo f1;
    Foo f2;
};
```

以及以下 shader 定义：

```slang
struct Foo {
    vec2 v;
};

struct UniformBuffer {
    Foo f1;
    Foo f2;
};
ConstantBuffer<UniformBuffer> ubo;
```

在这种情况下，`f2` 的偏移量将为 8，而它应该是 16，因为它是一个嵌套结构体。在这种情况下，你必须自己指定对齐方式：

```cpp
struct UniformBufferObject {
    Foo f1;
    alignas(16) Foo f2;
};
```

这些陷阱是始终明确指定对齐方式的充分理由，这样你就不会被对齐错误的奇怪症状所困惑。

```cpp
struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};
```

记得在删除 `foo` 字段后重新编译 shader。

---

## 多个 Descriptor Set（Multiple descriptor sets）

正如一些结构体和函数调用所暗示的，实际上可以同时绑定多个 descriptor set。在创建 pipeline layout 时，你需要为每个 descriptor set 指定一个 descriptor set layout。然后 shader 可以像这样引用特定的 descriptor set：

```slang
struct UniformBuffer {
};
ConstantBuffer<UniformBuffer> ubo;
```

你可以使用此功能将每个对象不同的描述符和共享的描述符放入单独的 descriptor set 中。在这种情况下，你可以避免在绘制调用之间重新绑定大多数描述符，这可能会更加高效。

在接下来的章节中，我们将在刚刚学到的基础上，为场景添加纹理。
