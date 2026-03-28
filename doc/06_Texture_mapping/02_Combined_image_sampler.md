# Combined Image Sampler（Combined image sampler）

> 原文：https://docs.vulkan.org/tutorial/latest/06_Texture_mapping/02_Combined_image_sampler.html

## 简介（Introduction）

我们在教程的 uniform buffers 部分第一次接触了描述符。在本章中，我们将了解一种新类型的描述符：combined image sampler（组合图像采样器）。这个描述符使着色器可以通过一个采样器对象来访问图像资源，就像我们在上一章创建的那样。

值得注意的是，Vulkan 在着色器中提供了通过不同描述符类型访问纹理的灵活性。虽然我们在本教程中使用 combined image sampler，但 Vulkan 也支持分开的采样器描述符（`VK_DESCRIPTOR_TYPE_SAMPLER`）和采样图像描述符（`VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE`）。使用独立描述符允许你对多个图像重用同一个采样器，或使用不同的采样参数访问同一个图像。在有许多纹理使用相同采样配置的场景中，这可能更高效。然而，combined image sampler 通常更方便，并且由于优化的缓存使用，在某些硬件上可以提供更好的性能。

我们将首先修改 descriptor set layout、descriptor pool 和 descriptor set，以包含这样一个 combined image sampler 描述符。之后，我们将为 `Vertex` 添加纹理坐标，并修改 fragment shader，从纹理中读取颜色，而不仅仅是对顶点颜色进行插值。

---

## 更新描述符（Updating the descriptors）

进入 `createDescriptorSetLayout` 函数，为 combined image sampler 描述符添加一个 `VkDescriptorSetLayoutBinding`。我们将其放在 uniform buffer 之后的 binding 中：

```cpp
std::array bindings = {
    vk::DescriptorSetLayoutBinding( 0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex, nullptr),
    vk::DescriptorSetLayoutBinding( 1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr)
};
vk::DescriptorSetLayoutCreateInfo layoutInfo({}, bindings.size(), bindings.data());
```

确保将 `stageFlags` 设置为指示我们打算在 fragment shader 中使用 combined image sampler 描述符。这是确定片元颜色的地方。也可以在 vertex shader 中使用纹理采样，例如通过高度图动态变形顶点网格。

我们还必须创建一个更大的 descriptor pool，通过在 `VkDescriptorPoolCreateInfo` 中添加另一个类型为 `VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER` 的 `VkPoolSize`，为 combined image sampler 的分配腾出空间。进入 `createDescriptorPool` 函数，修改它以包含此描述符的 `VkDescriptorPoolSize`：

```cpp
std::array poolSize {
    vk::DescriptorPoolSize( vk::DescriptorType::eUniformBuffer, MAX_FRAMES_IN_FLIGHT),
    vk::DescriptorPoolSize(  vk::DescriptorType::eCombinedImageSampler, MAX_FRAMES_IN_FLIGHT)
};
vk::DescriptorPoolCreateInfo poolInfo(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, MAX_FRAMES_IN_FLIGHT, poolSize);
```

descriptor pool 不足是验证层无法捕获的问题的一个好例子：从 Vulkan 1.1 开始，如果 pool 不够大，`vkAllocateDescriptorSets` 可能会以错误码 `VK_ERROR_POOL_OUT_OF_MEMORY` 失败，但驱动程序也可能尝试在内部解决这个问题。这意味着有时（取决于硬件、pool 大小和分配大小）驱动程序会允许我们执行超出 descriptor pool 限制的分配。其他时候，`vkAllocateDescriptorSets` 将失败并返回 `VK_ERROR_POOL_OUT_OF_MEMORY`。如果分配在某些机器上成功而在其他机器上失败，这可能特别令人沮丧。

由于 Vulkan 将分配责任转移给驱动程序，不再严格要求只分配创建 descriptor pool 时对应 `descriptorCount` 成员所指定数量的某种类型的描述符（`VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER` 等）。然而，这样做仍然是最佳实践，未来如果你启用了 Best Practice Validation，`VK_LAYER_KHRONOS_validation` 将对此类问题发出警告。

最后一步是将实际的图像和采样器资源绑定到 descriptor set 中的描述符。进入 `createDescriptorSets` 函数。

```cpp
for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vk::DescriptorBufferInfo bufferInfo{ .buffer = uniformBuffers[i], .offset = 0, .range = sizeof(UniformBufferObject) };
    vk::DescriptorImageInfo imageInfo{ .sampler = textureSampler, .imageView = textureImageView, .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal};
    ...
}
```

combined image sampler 结构的资源必须在 `VkDescriptorImageInfo` 结构体中指定，就像 uniform buffer 描述符的 buffer 资源在 `VkDescriptorBufferInfo` 结构体中指定一样。这就是上一章的对象汇聚在一起的地方。

```cpp
std::array descriptorWrites{
    vk::WriteDescriptorSet{ .dstSet = descriptorSets[i], .dstBinding = 0, .dstArrayElement = 0, .descriptorCount = 1,
        .descriptorType = vk::DescriptorType::eUniformBuffer, .pBufferInfo = &bufferInfo },
    vk::WriteDescriptorSet{ .dstSet = descriptorSets[i], .dstBinding = 1, .dstArrayElement = 0, .descriptorCount = 1,
        .descriptorType = vk::DescriptorType::eCombinedImageSampler, .pImageInfo = &imageInfo }
};
device.updateDescriptorSets(descriptorWrites, {});
```

描述符必须使用此图像信息进行更新，就像 buffer 一样。这次我们使用 `pImageInfo` 数组而不是 `pBufferInfo`。描述符现在已准备好供着色器使用！

---

## 纹理坐标（Texture coordinates）

纹理映射还缺少一个重要的要素，那就是每个顶点的实际纹理坐标，通常称为"uv 坐标"。纹理坐标决定了图像实际如何映射到几何体上。

```cpp
struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;
    glm::vec2 texCoord;
    static vk::VertexInputBindingDescription getBindingDescription() {
        return { 0, sizeof(Vertex), vk::VertexInputRate::eVertex };
    }
    static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions() {
        return {
            vk::VertexInputAttributeDescription( 0, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, pos) ),
            vk::VertexInputAttributeDescription( 1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color) ),
            vk::VertexInputAttributeDescription( 2, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, texCoord) )
        };
    }
};
```

修改 `Vertex` 结构体，为纹理坐标添加一个 `vec2`。同时确保添加一个 `VkVertexInputAttributeDescription`，以便我们可以在 vertex shader 中将纹理坐标作为输入访问。这是将纹理坐标传递到 fragment shader 以在正方形表面上进行插值所必需的。

```cpp
const std::vector<Vertex> vertices = {
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
    {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}
};
```

在本教程中，我将简单地使用从左上角 0, 0 到右下角 1, 1 的坐标来用纹理填充正方形。你可以随意尝试不同的坐标。尝试使用低于 0 或高于 1 的坐标来查看寻址模式的效果！

---

## 着色器（Shaders）

最后一步是修改着色器，从纹理中采样颜色。我们首先需要修改 vertex shader，将纹理坐标传递到 fragment shader：

```slang
struct VSInput {
    float2 inPos;
    float3 inColor;
    float2 inTexCoord;
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
    float3 fragColor;
    float2 fragTexCoord;
};
[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.pos = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(input.inPos, 0.0, 1.0))));
    output.fragColor = input.inColor;
    output.fragTexCoord = input.inTexCoord;
    return output;
}
Sampler2D texture;
[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
   return texture.Sample(vertIn.fragTexCoord);
}
```

记得重新编译着色器！你应该看到如下图所示的内容。绿色通道代表水平坐标，红色通道代表垂直坐标。黑色和黄色的角落确认纹理坐标在正方形上从 0, 0 正确插值到 1, 1。使用颜色可视化数据是着色器编程中等同于 `printf` 调试的方法，虽然不是最好的选择！

![texcoord visualization](https://docs.vulkan.org/tutorial/latest/_images/images/texcoord_visualization.png)

在 Slang 中，`Sampler2D` 代表一个 combined image sampler 描述符。在 fragment shader 中添加对它的引用：

```slang
Sampler2D texture;
```

对于其他类型的图像，有等效的 `sampler1D` 和 `sampler3D` 类型。确保在这里使用正确的 binding。

```slang
[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
   return texture.Sample(vertIn.fragTexCoord);
}
```

纹理使用内置的 `texture` 函数进行采样。它接受一个采样器和坐标作为参数。采样器在后台自动处理过滤和变换操作。现在运行应用程序，你应该能看到正方形上的纹理：

![texture on square](https://docs.vulkan.org/tutorial/latest/_images/images/texture_on_square.png)

尝试通过将纹理坐标缩放到大于 1 的值来试验寻址模式。例如，当使用 `VK_SAMPLER_ADDRESS_MODE_REPEAT` 时，以下 fragment shader 会产生下图的结果：

```slang
[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
   return texture.Sample(vertIn.fragTexCoord);
}
```

![texture on square repeated](https://docs.vulkan.org/tutorial/latest/_images/images/texture_on_square_repeated.png)

你还可以使用顶点颜色来处理纹理颜色：

```slang
[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
   return float4(vertIn.fragColor * texture.Sample(vertIn.fragTexCoord).rgb, 1.0);
}
```

在这里我将 RGB 和 alpha 通道分开，以避免缩放 alpha 通道。

![texture on square colorized](https://docs.vulkan.org/tutorial/latest/_images/images/texture_on_square_colorized.png)

你现在知道如何在着色器中访问图像了！这与写入 framebuffer 的图像结合使用时是一项非常强大的技术。你可以将这些图像用作输入，实现各种酷炫的效果，如后处理和 3D 世界中的相机显示。在下一章中，我们将学习如何添加深度缓冲，以正确地对物体进行排序。
