# Vertex Input 描述（Vertex Input Description）

> 原文：[https://docs.vulkan.org/tutorial/latest/04_Vertex_buffers/00_Vertex_input_description.html](https://docs.vulkan.org/tutorial/latest/04_Vertex_buffers/00_Vertex_input_description.html)

## 介绍（Introduction）

在接下来的几章中，我们将把 vertex shader 中硬编码的顶点数据替换为内存中的 vertex buffer。我们将从最简单的方法开始——创建一个 CPU 可见的 buffer，并使用 `memcpy` 将顶点数据直接复制进去。之后我们还会了解如何使用 staging buffer 将顶点数据复制到高性能内存中。

## Vertex Shader

首先，修改 vertex shader，使其不再在 shader 代码本身中包含顶点数据。Vertex shader 通过以正确顺序声明在一个 struct 中的方式，从 vertex buffer 接收输入。

```hlsl
struct VSInput {
    float2 inPosition;
    float3 inColor;
};

struct VSOutput
{
    float4 pos : SV_Position;
    float3 color;
};

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.pos = float4(input.inPosition, 0.0, 1.0);
    output.color = input.inColor;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
    return float4(vertIn.color, 1.0);
}
```

`inPosition` 和 `inColor` 变量是 _vertex attributes_（顶点属性）。它们是在 vertex buffer 中逐顶点指定的属性，就像我们之前用两个数组手动为每个顶点指定位置和颜色一样。

## 顶点数据（Vertex Data）

我们要将顶点数据从 shader 代码移到程序代码中的一个数组里。首先引入 GLM 库，它为我们提供了与线性代数相关的类型，如向量和矩阵。我们将使用这些类型来指定位置向量和颜色向量。

```cpp
#include <glm/glm.hpp>
```

创建一个名为 `Vertex` 的新 struct，其中包含我们将在 vertex shader 中使用的两个属性：

```cpp
struct Vertex
{
    glm::vec2 pos;
    glm::vec3 color;
};
```

GLM 为我们提供了与 shader 语言中使用的向量类型完全匹配的 C++ 类型。

```cpp
const std::vector<Vertex> vertices = {
    {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
    {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
};
```

现在使用 `Vertex` struct 来指定一个顶点数据数组。我们使用的位置值和颜色值与之前完全相同，但现在它们被合并到了一个顶点数组中。这被称为 _interleaving_（交错）vertex attributes。

## Binding Descriptions

下一步是告诉 Vulkan，将数据上传到 GPU 内存后，如何将这种数据格式传递给 vertex shader。需要两种类型的 struct 来传达这些信息。

第一个 struct 是 `vk::VertexInputBindingDescription`，我们将在 `Vertex` struct 中添加一个成员函数来用正确的数据填充它。

```cpp
struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;

    static vk::VertexInputBindingDescription getBindingDescription() {
        return { 0, sizeof(Vertex), vk::VertexInputRate::eVertex };
    }
};
```

vertex binding 描述了在整个顶点处理过程中从内存加载数据的速率。它指定了数据条目之间的字节数，以及是在每个顶点之后还是在每个 instance 之后移动到下一个数据条目。

我们所有的逐顶点数据都打包在一个数组中，因此我们只有一个 binding。`binding` 参数指定了该 binding 在 binding 数组中的索引。`stride` 参数指定了从一个条目到下一个条目的字节数，`inputRate` 参数可以取以下值之一：

- `vk::VertexInputRate::eVertex`：每处理完一个顶点后移动到下一个数据条目
- `vk::VertexInputRate::eInstance`：每处理完一个 instance 后移动到下一个数据条目

我们不会使用 instanced rendering，因此采用逐顶点数据的方式。

## Attribute Descriptions

描述如何处理 vertex input 的第二个 struct 是 `vk::VertexInputAttributeDescription`。我们将在 `Vertex` 中添加另一个辅助函数来填充这些 struct。

```cpp
#include <array>

...

static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions() {
    return {
        vk::VertexInputAttributeDescription( 0, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, pos) ),
        vk::VertexInputAttributeDescription( 1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color) )
    };
}
```

正如函数原型所示，我们需要两个这样的 struct。一个 attribute description struct 描述了如何从来自某个 binding description 的顶点数据块中提取某个 vertex attribute。我们有两个属性——位置和颜色，因此需要两个 attribute description struct。

`binding` 参数告诉 Vulkan 逐顶点数据来自哪个 binding。`location` 参数引用 vertex shader 中输入变量的 `location` 指令。vertex shader 中 location 为 `0` 的输入是位置，它有两个 32 位浮点数分量。

`format` 参数描述了该属性的数据类型。有些令人困惑的是，format 使用与颜色 format 相同的枚举值来指定。以下 shader 类型和 format 通常配合使用：

- `float`：`vk::Format::eR32Sfloat`
- `float2`：`vk::Format::eR32G32Sfloat`
- `float3`：`vk::Format::eR32G32B32Sfloat`
- `float4`：`vk::Format::eR32G32B32A32Sfloat`

如你所见，应使用颜色通道数量与 shader 数据类型分量数量相匹配的 format。允许使用比 shader 中分量数更多的通道数，但多余的通道将被静默丢弃。如果通道数少于分量数，则 BGA 分量将使用默认值 `(0, 0, 1)`。颜色类型（`Sfloat`、`Uint`、`Sint`）和位宽也应与 shader 输入的类型匹配。以下是一些示例：

- `int2`：`vk::Format::eR32G32Sint`，由两个 32 位有符号整数组成的 2 分量向量
- `uint4`：`vk::Format::eR32G32B32A32Uint`，由四个 32 位无符号整数组成的 4 分量向量
- `double`：`vk::Format::eR64Sfloat`，双精度（64 位）浮点数

`format` 参数隐式定义了 attribute 数据的字节大小，`offset` 参数指定了从逐顶点数据起始位置到读取位置的字节数。binding 每次加载一个 `Vertex`，而位置 attribute（`pos`）在该 struct 起始处的偏移量为 `0` 字节。这通过 `offsetof` 宏自动计算得出。

颜色 attribute 的描述方式与之基本相同。

## Pipeline Vertex Input

现在我们需要配置 graphics pipeline，通过在 `createGraphicsPipeline` 中引用上述两种描述 struct，使其能够接受这种格式的顶点数据。找到 `vertexInputInfo` struct 并修改它以引用这两个描述：

```cpp
auto                                   bindingDescription    = Vertex::getBindingDescription();
auto                                   attributeDescriptions = Vertex::getAttributeDescriptions();
vk::PipelineVertexInputStateCreateInfo vertexInputInfo{ .vertexBindingDescriptionCount   = 1,
                                                        .pVertexBindingDescriptions      = &bindingDescription,
                                                        .vertexAttributeDescriptionCount = static_cast<uint32_t>( attributeDescriptions.size() ),
                                                        .pVertexAttributeDescriptions    = attributeDescriptions.data() };
```

pipeline 现在已准备好接受 `vertices` 容器格式的顶点数据，并将其传递给我们的 vertex shader。下一步是创建一个 vertex buffer，并将顶点数据移入其中，以便 GPU 能够访问它。
