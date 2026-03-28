# Descriptor Set Layout 与 Uniform Buffer（Descriptor layout and buffer）

> 原文：https://docs.vulkan.org/tutorial/latest/05_Uniform_buffers/00_Descriptor_set_layout_and_buffer.html

## 简介（Introduction）

我们现在已经能够为每个顶点向 vertex shader 传递任意属性了，但全局变量又该如何处理呢？

从本章开始，我们将进入 3D 图形的世界，这需要用到 model-view-projection 矩阵。我们可以将其作为顶点数据传入，但这样会浪费内存，而且每当变换发生变化时就需要更新 vertex buffer。变换很可能每一帧都会改变。

在 Vulkan 中，处理这个问题的正确方式是使用资源描述符（resource descriptor）。描述符是让 shader 自由访问缓冲区（buffer）和图像（image）等资源的一种方式。我们将建立一个包含变换矩阵的 buffer，并让 vertex shader 通过描述符来访问它。

使用描述符由三个部分组成：

- 在 pipeline 创建期间指定 descriptor set layout
- 从 descriptor pool 中分配 descriptor set
- 在渲染时绑定 descriptor set

descriptor set layout 指定了 pipeline 将要访问的资源类型，就像 render pass 指定将要访问的 attachment 类型一样。descriptor set 指定将被绑定到描述符的实际 buffer 或 image 资源，就像 framebuffer 指定要绑定到 render pass attachment 的实际 image view 一样。然后，descriptor set 在绘制命令中被绑定，就像 vertex buffer 和 framebuffer 一样。

描述符有很多种类型，但在本章中我们将使用 uniform buffer object（UBO）。我们将在后续章节中介绍其他类型的描述符，但基本流程是相同的。

假设我们希望 vertex shader 拥有的数据以 C 结构体表示如下：

```cpp
struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};
```

然后我们可以将数据复制到 `VkBuffer` 中，并在 vertex shader 中通过 uniform buffer object 描述符访问它，如下所示：

```slang
struct VSInput {
    float2 inPosition;
    float3 inColor;
};

struct UniformBuffer {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};

ConstantBuffer<UniformBuffer> ubo;

struct VSOutput
{
    float4 pos : SV_Position;
    float3 color;
};

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.pos = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(input.inPosition, 0.0, 1.0))));
    output.color = input.inColor;
    return output;
}
```

我们将在每一帧更新 model、view 和 projection 矩阵，使上一章中的矩形在 3D 空间中旋转。

---

## Vertex Shader

按照上述方式修改 vertex shader，使其包含 uniform buffer object。

我将假设你已经熟悉 MVP 变换。如果还不熟悉，请参阅第一章中提到的资源。

```slang
struct VSInput {
    float2 inPosition;
    float3 inColor;
};

struct UniformBuffer {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};

ConstantBuffer<UniformBuffer> ubo;

struct VSOutput
{
    float4 pos : SV_Position;
    float3 color;
};

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.pos = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(input.inPosition, 0.0, 1.0))));
    output.color = input.inColor;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
    return float4(vertIn.color, 1.0);
}
```

带有 `SV_Position` 的那一行被修改为使用变换矩阵来计算裁剪坐标中的最终位置。与 2D 三角形不同，裁剪坐标的最后一个分量可能不是 1，这将在转换为屏幕上最终的归一化设备坐标时产生除法。这在透视投影中被称为透视除法（perspective division），对于使近处物体看起来比远处物体更大至关重要。

---

## Descriptor Set Layout

下一步是在 C++ 端定义 UBO，并告知 Vulkan vertex shader 中的这个描述符。

```cpp
struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};
```

我们可以使用 GLM 中的数据类型来精确匹配 shader 中的定义。矩阵中的数据与 shader 期望的方式是二进制兼容的，因此我们之后可以直接将 `UniformBufferObject` memcpy 到 `VkBuffer` 中。

我们需要为 shader 中使用的每个描述符绑定提供详细信息，就像我们必须为每个顶点属性及其 location index 所做的那样。我们将建立一个新函数 `createDescriptorSetLayout` 来定义所有这些信息。它应该在 pipeline 创建之前调用，因为我们在那里会用到它。

```cpp
void initVulkan() {
    ...
    createDescriptorSetLayout();
    createGraphicsPipeline();
    ...
}
...
void createDescriptorSetLayout() {
}
```

每个绑定都需要通过 `VkDescriptorSetLayoutBinding` 结构体来描述。

```cpp
void createDescriptorSetLayout() {
    vk::DescriptorSetLayoutBinding uboLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex, nullptr);
}
```

前两个字段指定了 shader 中使用的 binding 以及描述符的类型，即 uniform buffer object。shader 变量可以表示一个 uniform buffer object 数组，`descriptorCount` 指定数组中值的数量。例如，这可以用于为骨骼动画中的每根骨骼指定一个变换。我们的 MVP 变换在单个 uniform buffer object 中，所以我们使用 `descriptorCount` 为 1。

我们还需要指定在哪些 shader 阶段将引用该描述符。`stageFlags` 字段可以是 `VkShaderStageFlagBits` 值的组合，或者是 `VK_SHADER_STAGE_ALL_GRAPHICS`。在我们的例子中，我们只在 vertex shader 中引用该描述符。

`pImmutableSamplers` 字段只与图像采样相关的描述符有关，我们稍后会看到。现在可以将其保留为默认值。

所有描述符绑定被合并到一个 `VkDescriptorSetLayout` 对象中。在 `pipelineLayout` 上方定义一个新的类成员：

```cpp
vk::raii::DescriptorSetLayout descriptorSetLayout = nullptr;
vk::raii::PipelineLayout pipelineLayout = nullptr;
```

然后可以使用 `vkCreateDescriptorSetLayout` 创建它。该函数接受一个简单的 `VkDescriptorSetLayoutCreateInfo`，其中包含绑定数组：

```cpp
vk::DescriptorSetLayoutBinding uboLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex, nullptr);
vk::DescriptorSetLayoutCreateInfo layoutInfo{.bindingCount = 1, .pBindings = &uboLayoutBinding};
descriptorSetLayout = vk::raii::DescriptorSetLayout(device, layoutInfo);
```

我们需要在 pipeline 创建期间指定 descriptor set layout，以告知 Vulkan shader 将使用哪些描述符。descriptor set layout 在 pipeline layout 对象中指定。

修改 `VkPipelineLayoutCreateInfo` 以引用 layout 对象：

```cpp
vk::PipelineLayoutCreateInfo pipelineLayoutInfo{ .setLayoutCount = 1, .pSetLayouts = &*descriptorSetLayout, .pushConstantRangeCount = 0 };
```

你可能想知道，为什么可以在这里指定多个 descriptor set layout，因为单个 layout 已经包含了所有绑定。我们将在下一章回到这个问题，届时我们将了解 descriptor pool 和 descriptor set。

---

## Uniform Buffer

在下一章中，我们将指定包含 shader UBO 数据的 buffer，但我们首先需要创建这个 buffer。由于我们将每帧向 uniform buffer 复制新数据，使用 staging buffer 实际上没有意义。它在这种情况下只会增加额外开销，可能反而降低性能。

我们应该有多个 buffer，因为多帧可能同时在飞行（in flight），我们不希望在前一帧仍在读取 buffer 时就更新 buffer 来准备下一帧！因此，我们需要拥有与飞行中帧数一样多的 uniform buffer，并写入当前未被 GPU 读取的 uniform buffer。

为此，为 `uniformBuffers`、`uniformBuffersMemory` 添加新的类成员：

```cpp
vk::raii::Buffer indexBuffer = nullptr;
vk::raii::DeviceMemory indexBufferMemory = nullptr;

std::vector<vk::raii::Buffer> uniformBuffers;
std::vector<vk::raii::DeviceMemory> uniformBuffersMemory;
std::vector<void*> uniformBuffersMapped;
```

类似地，创建一个新函数 `createUniformBuffers`，在 `createIndexBuffer` 之后调用并分配 buffer：

```cpp
void initVulkan() {
    ...
    createVertexBuffer();
    createIndexBuffer();
    createUniformBuffers();
    ...
}
...
void createUniformBuffers() {
    uniformBuffers.clear();
    uniformBuffersMemory.clear();
    uniformBuffersMapped.clear();

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vk::DeviceSize bufferSize = sizeof(UniformBufferObject);
        vk::raii::Buffer buffer({});
        vk::raii::DeviceMemory bufferMem({});
        createBuffer(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, buffer, bufferMem);
        uniformBuffers.emplace_back(std::move(buffer));
        uniformBuffersMemory.emplace_back(std::move(bufferMem));
        uniformBuffersMapped.emplace_back( uniformBuffersMemory[i].mapMemory(0, bufferSize));
    }
}
```

我们在创建后立即使用 `vkMapMemory` 映射 buffer，以获得一个指针，稍后可以通过该指针写入数据。buffer 在应用程序的整个生命周期内都保持映射到这个指针。这种技术被称为"持久映射"（persistent mapping），在所有 Vulkan 实现上都有效。不必在每次需要更新时都映射 buffer 可以提高性能，因为映射并非免费的。

---

## 更新 Uniform 数据（Updating uniform data）

创建一个新函数 `updateUniformBuffer`，并在 `drawFrame` 函数中、提交下一帧之前调用它：

```cpp
void drawFrame() {
    ...
    updateUniformBuffer(frameIndex);
    ...
    const vk::SubmitInfo   submitInfo{.waitSemaphoreCount   = 1,
                                      .pWaitSemaphores      = &*presentCompleteSemaphores[frameIndex],
                                      .pWaitDstStageMask    = &waitDestinationStageMask,
                                      .commandBufferCount   = 1,
                                      .pCommandBuffers      = &*commandBuffers[frameIndex],
                                      .signalSemaphoreCount = 1,
                                      .pSignalSemaphores    = &*renderFinishedSemaphores[imageIndex]};
    ...
}
...
void updateUniformBuffer(uint32_t currentImage) {
}
```

该函数将每帧生成一个新的变换，使几何体旋转。我们需要引入两个新的头文件来实现此功能：

```cpp
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
```

`glm/gtc/matrix_transform.hpp` 头文件暴露了可用于生成模型变换（如 `glm::rotate`）、视图变换（如 `glm::lookAt`）和投影变换（如 `glm::perspective`）的函数。`chrono` 标准库头文件暴露了用于精确计时的函数。我们将用它来确保几何体以每秒 90 度的速度旋转，而不受帧率影响。

```cpp
void updateUniformBuffer(uint32_t currentImage) {
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
}
```

`updateUniformBuffer` 函数将首先执行一些逻辑，以浮点精度计算自渲染开始以来经过的秒数。

我们现在将在 uniform buffer object 中定义 model、view 和 projection 变换。model 旋转将是一个使用 `time` 变量绕 Z 轴的简单旋转：

```cpp
UniformBufferObject ubo{};
ubo.model = rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
```

`glm::rotate` 函数以现有变换、旋转角度和旋转轴为参数。`glm::mat4(1.0f)` 构造函数返回一个单位矩阵。使用 `time * glm::radians(90.0f)` 作为旋转角度可以实现每秒旋转 90 度的目的。

```cpp
ubo.view = lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
```

对于 view 变换，我决定从 45 度角从上方观察几何体。`glm::lookAt` 函数以眼睛位置、中心位置和上方向轴为参数。

```cpp
ubo.proj = glm::perspective(glm::radians(45.0f), static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height), 0.1f, 10.0f);
```

我选择使用垂直视角为 45 度的透视投影。其他参数是宽高比、近裁剪面和远裁剪面。使用当前 swap chain 的 extent 来计算宽高比非常重要，这样可以在窗口缩放后考虑到新的宽度和高度。

```cpp
ubo.proj[1][1] *= -1;
```

GLM 最初是为 OpenGL 设计的，在 OpenGL 中裁剪坐标的 Y 坐标是反转的。最简单的补偿方法是在 projection 矩阵中翻转 Y 轴缩放因子的符号。如果不这样做，图像将被上下颠倒地渲染。

现在所有变换都已定义，我们可以将 uniform buffer object 中的数据复制到当前的 uniform buffer 中。这与我们处理 vertex buffer 的方式完全相同，只是没有 staging buffer。如前所述，我们只映射一次 uniform buffer，因此可以直接向其写入而无需再次映射：

```cpp
memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
```

以这种方式使用 UBO 并不是向 shader 传递频繁变化值的最高效方式。向 shader 传递少量数据缓冲区的更高效方式是 push constant。我们可能会在未来的章节中了解这些内容。

在下一章中，我们将了解 descriptor set，它将实际把 `VkBuffer` 绑定到 uniform buffer 描述符，以便 shader 能够访问这些变换数据。
