# 加载模型（Loading Models）

> 原文：https://docs.vulkan.org/tutorial/latest/08_Loading_models.html

## 简介（Introduction）

你的程序现在已经可以渲染带纹理的 3D 网格了，但 `vertices` 和 `indices` 数组中的当前几何体还不够有趣。在本章中，我们将扩展程序，从实际的模型文件中加载顶点和索引，让显卡真正做一些工作。

许多图形 API 教程会在这样的章节中让读者自己编写 OBJ 加载器。这样做的问题是，任何稍微有趣一点的 3D 应用程序很快就会需要该文件格式不支持的特性，例如骨骼动画。在本章中，我们将从 OBJ 模型加载网格数据，但我们会更专注于将网格数据与程序本身集成，而非加载文件的细节。

---

## 库（Library）

我们将使用 tinyobjloader 库从 OBJ 文件加载顶点和面。它快速且易于集成，因为它和 stb_image 一样是单文件库。这在开发环境章节中已经提到，应该已经是本教程此部分依赖的一部分。

---

## 示例网格（Sample mesh）

在本章中，我们还不会启用光照，因此使用一个在纹理中烘焙了光照的示例模型会有所帮助。找到此类模型的一个简单方法是在 Sketchfab 上寻找 3D 扫描作品。该网站上的许多模型以 OBJ 格式提供，并带有宽松的许可证。

在本教程中，我决定使用 nigelgoh 制作的 Viking room 模型（CC BY 4.0）。我调整了模型的大小和方向，使其可以直接替换当前的几何体：

- `viking_room.obj`
- `viking_room.png`

你可以随意使用自己的模型，但请确保它只由一种材质组成，且尺寸约为 1.5 × 1.5 × 1.5 单位。如果尺寸更大，则需要修改 view 矩阵。

在 `shaders` 和 `textures` 目录旁边新建一个 `models` 目录，将模型文件放入其中，并将纹理图像放入 `textures` 目录。

在你的程序中添加两个新的配置变量，用于定义模型和纹理的路径：

```cpp
constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;

const std::string MODEL_PATH = "models/viking_room.obj";
const std::string TEXTURE_PATH = "textures/viking_room.png";
```

并更新 `createTextureImage`，使用这个路径变量：

```cpp
stbi_uc* pixels = stbi_load(TEXTURE_PATH.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
```

---

## 加载顶点和索引（Loading vertices and indices）

现在我们将从模型文件中加载顶点和索引，因此应该删除全局的 `vertices` 和 `indices` 数组。将它们替换为类成员中的非 const 容器：

```cpp
std::vector<Vertex> vertices;
std::vector<uint32_t> indices;
vk::raii::Buffer vertexBuffer = nullptr;
vk::raii::DeviceMemory vertexBufferMemory = nullptr;
vk::raii::Buffer indexBuffer = nullptr;
vk::raii::DeviceMemory indexBufferMemory = nullptr;
```

你应该将 `indices` 的类型从 `uint16_t` 改为 `uint32_t`，因为顶点数量将会远超 65535。同时记得修改 `vkCmdBindIndexBuffer` 的参数：

```cpp
commandBuffers[frameIndex]->bindIndexBuffer( **indexBuffer, 0, vk::IndexType::eUint32 );
```

tinyobjloader 库的引入方式与 STB 库相同。引入 `tiny_obj_loader.h` 头文件，并确保在某个源文件中定义 `TINYOBJLOADER_IMPLEMENTATION`，以包含函数体并避免链接错误：

```cpp
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
```

现在我们将编写一个 `loadModel` 函数，使用这个库将网格的顶点数据填充到 `vertices` 和 `indices` 容器中。它应该在 vertex buffer 和 index buffer 创建之前的某个地方调用：

```cpp
void initVulkan() {
    ...
    loadModel();
    createVertexBuffer();
    createIndexBuffer();
    ...
}
...
void loadModel() {
}
```

通过调用 `tinyobj::LoadObj` 函数，将模型加载到库的数据结构中：

```cpp
void loadModel() {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, MODEL_PATH.c_str())) {
        throw std::runtime_error(warn + err);
    }
}
```

OBJ 文件由位置、法线、纹理坐标和面组成。面由任意数量的顶点组成，每个顶点通过索引引用一个位置、法线和/或纹理坐标属性。这使得不仅可以复用完整的顶点，还可以复用单个属性。

`attrib` 容器将所有位置、法线和纹理坐标分别存放在 `attrib.vertices`、`attrib.normals` 和 `attrib.texcoords` 向量中。`shapes` 容器包含所有独立对象及其面。每个面由一个顶点数组组成，每个顶点包含位置、法线和纹理坐标属性的索引。

OBJ 模型还可以为每个面定义材质和纹理，但我们将忽略这些。`err` 字符串包含加载文件时发生的错误，`warn` 字符串包含警告，例如缺少材质定义。只有当 `LoadObj` 函数返回 `false` 时才真正加载失败。

如前所述，OBJ 文件中的面实际上可以包含任意数量的顶点，而我们的应用程序只能渲染三角形。幸运的是，`LoadObj` 有一个可选参数可以自动将此类面三角化，默认情况下已启用。

我们将把文件中的所有面合并到一个模型中，只需遍历所有形状：

```cpp
for (const auto& shape : shapes) {
}
```

三角化特性已经确保每个面有三个顶点，因此我们现在可以直接遍历顶点，并将它们直接转储到我们的 `vertices` 向量中：

```cpp
for (const auto& shape : shapes) {
    for (const auto& index : shape.mesh.indices) {
        Vertex vertex{};
        vertices.push_back(vertex);
        indices.push_back(indices.size());
    }
}
```

为简单起见，我们暂时假设每个顶点都是唯一的，因此使用简单的自增索引。`index` 变量的类型为 `tinyobj::index_t`，包含 `vertex_index`、`normal_index` 和 `texcoord_index` 成员。我们需要使用这些索引在 `attrib` 数组中查找实际的顶点属性：

```cpp
vertex.pos = {
    attrib.vertices[3 * index.vertex_index + 0],
    attrib.vertices[3 * index.vertex_index + 1],
    attrib.vertices[3 * index.vertex_index + 2]
};

vertex.texCoord = {
    attrib.texcoords[2 * index.texcoord_index + 0],
    attrib.texcoords[2 * index.texcoord_index + 1]
};

vertex.color = {1.0f, 1.0f, 1.0f};
```

不幸的是，`attrib.vertices` 数组是一个 `float` 值的数组，而不是 `glm::vec3` 之类的东西，因此你需要将索引乘以 3。类似地，每条纹理坐标条目有两个分量。偏移量 0、1 和 2 用于访问 X、Y 和 Z 分量，或者在纹理坐标的情况下访问 U 和 V 分量。

现在启用优化（例如 Visual Studio 中的 Release 模式，或 GCC 的 `-O3` 编译器标志）运行程序。这是必要的，否则加载模型会非常慢。你应该看到类似以下的画面：

![inverted texture coordinates](https://docs.vulkan.org/tutorial/latest/_images/images/inverted_texture_coordinates.png)

太好了，几何体看起来是正确的，但纹理是怎么回事？OBJ 格式假定使用一个坐标系，其中垂直坐标 0 表示图像的底部，然而我们将图像以从上到下的方向上传到 Vulkan，其中 0 表示图像的顶部。

通过翻转纹理坐标的垂直分量来解决这个问题：

```cpp
vertex.texCoord = {
    attrib.texcoords[2 * index.texcoord_index + 0],
    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
};
```

再次运行程序，你现在应该看到正确的结果：

![drawing model](https://docs.vulkan.org/tutorial/latest/_images/images/drawing_model.png)

所有这些辛苦的工作终于在这样的演示中开始得到回报了！随着模型旋转，你可能会注意到背面（墙壁的背面）看起来有些奇怪。这是正常的，只是因为模型本来就不是为从那一侧观看而设计的。

---

## 顶点去重（Vertex deduplication）

不幸的是，我们还没有真正利用好 index buffer。`vertices` 向量包含大量重复的顶点数据，因为许多顶点被包含在多个三角形中。我们应该只保留唯一的顶点，并使用 index buffer 在它们出现时重用它们。

一种直接的实现方法是使用 `map` 或 `unordered_map` 来跟踪唯一顶点及其对应的索引：

```cpp
#include <unordered_map>
...
std::unordered_map<Vertex, uint32_t> uniqueVertices{};

for (const auto& shape : shapes) {
    for (const auto& index : shape.mesh.indices) {
        Vertex vertex{};
        ...
        if (uniqueVertices.count(vertex) == 0) {
            uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
            vertices.push_back(vertex);
        }
        indices.push_back(uniqueVertices[vertex]);
    }
}
```

每次我们从 OBJ 文件读取一个顶点时，都会检查是否已经见过一个具有完全相同位置和纹理坐标的顶点。如果没有，我们将其添加到 `vertices` 中，并将其索引存储在 `uniqueVertices` 容器中。之后，我们将新顶点的索引添加到 `indices` 中。如果我们之前已经见过完全相同的顶点，则在 `uniqueVertices` 中查找其索引，并将该索引存储到 `indices` 中。

程序现在会编译失败，因为使用像我们的 `Vertex` 结构体这样的用户自定义类型作为哈希表的键，需要我们实现两个函数：相等性测试和哈希计算。前者通过重载 `Vertex` 结构体中的 `==` 运算符来轻松实现：

```cpp
bool operator==(const Vertex& other) const {
    return pos == other.pos && color == other.color && texCoord == other.texCoord;
}
```

`Vertex` 的哈希函数通过为 `std::hash<T>` 指定模板特化来实现。哈希函数是一个复杂的话题，但 cppreference.com 推荐以下方法，通过组合结构体的字段来创建质量合理的哈希函数：

```cpp
namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex.pos) ^
                   (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
                   (hash<glm::vec2>()(vertex.texCoord) << 1);
        }
    };
}
```

这段代码应该放在 `Vertex` 结构体外部。GLM 类型的哈希函数需要通过以下头文件引入：

```cpp
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
```

哈希函数定义在 `gtx` 文件夹中，这意味着它在技术上仍是 GLM 的一个实验性扩展。因此，你需要定义 `GLM_ENABLE_EXPERIMENTAL` 才能使用它。这意味着 API 可能会随着 GLM 的新版本而改变，但实际上该 API 非常稳定。

你现在应该能够成功编译并运行程序。如果你查看 `vertices` 的大小，会发现它从 1,500,000 缩减到了 265,645！这意味着每个顶点平均在约 6 个三角形中被复用。这为我们节省了大量的 GPU 内存。

在下一章中，我们将学习一种改善纹理渲染的技术。
