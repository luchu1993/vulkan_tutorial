# Shader 模块（Shader modules）

> 原文：[https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/02_Graphics_pipeline_basics/01_Shader_modules.html](https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/02_Graphics_pipeline_basics/01_Shader_modules.html)

## 简介（Introduction）

与早期的 API 不同，Vulkan 中的 shader 代码必须以字节码格式指定，而非 GLSL、Slang 和 HLSL 这样人类可读的语法。这种字节码格式被称为 **SPIR-V**，专为 Vulkan（一个 Khronos API）设计，同时也适用于 OpenCL（另一个 Khronos API）。它是一种可用于编写图形和计算 shader 的格式。

使用字节码格式的优势在于，GPU 厂商编写的将 shader 代码转化为本地 GPU 指令的编译器显著减少了复杂度。过去的经验表明，对于 GLSL 这样的人类可读语法，一些 GPU 厂商在解读标准时相当随意。如果你碰巧使用了这些厂商的 GPU 来编写非平凡的 shader，那么其他厂商的驱动程序很可能会因语法错误而拒绝你的代码，或者更糟糕的是，你的 shader 会因为编译器 bug 而产生不同的运行结果。使用 SPIR-V 这样直接的字节码格式，有望避免此类问题。

然而，这并不意味着我们需要手写这种字节码。Khronos 发布了他们自己的厂商无关编译器，可以将 Slang 编译为 SPIR-V。**Slang 是一种具有 C 风格语法的 shading 语言**，包含向量（vectors）和矩阵（matrices）等原始类型以及用于图形操作的内置函数。Slang 还具有一些功能，例如支持将各种 shader stage 包含在单个源文件中，并支持自动微分（automatic differentiation）。

向量类型被称为 `float`，后跟表示元素数量的数字。例如，3D 位置会存储在 `float3` 中。你也可以通过 `.x`、`.y` 和 `.z` 等字段访问单个分量，还可以通过 swizzle 操作同时创建新向量，例如 `float3(1.0, 2.0, 3.0).xy` 会得到 `float2`。矩阵类型（如 `float4x4`）采用行优先格式定义。

Vulkan SDK 包含 **slangc**，这是一个将 Slang 编译为 SPIR-V 字节码的工具。本教程将使用 Slang 作为 shader 语言，但也会为每个 shader 提供 GLSL 版本。

除了 Slang 的 slangc 之外，Google 还开发了 **glslc**——一款将 GLSL 编译为 SPIR-V 的编译器。glslc 使用与 GCC 和 Clang 相同的参数格式，并包含一些额外功能，例如 include 支持。两者都已包含在 Vulkan SDK 中，因此你无需额外下载任何内容。

GLSL 是一种使用 C 风格语法的 shading 语言。用它编写的程序有一个 `main` 函数，该函数对每个对象调用一次。GLSL 使用全局变量来处理输入和输出，而不是将输入参数传递并返回值。该语言包含许多有助于图形编程的内置功能，例如内置的叉积、矩阵-向量积和三维向量反射等。向量类型被称为 `vec`，后跟表示元素数量的数字，例如 3D 位置会存储在 `vec3` 中。

本节将编写两个 shader：一个 vertex shader 和一个 fragment shader，并将它们编译为 SPIR-V 字节码，然后加载到程序中，最终将它们插入 graphics pipeline 中。

## Vertex shader

Vertex shader 处理每个传入的顶点。它将顶点的属性（如世界位置、颜色、法线和纹理坐标）作为输入，输出是 clip coordinate 中的最终位置以及需要传递给 fragment shader 的属性（如颜色和纹理坐标）。这些值随后会在 fragment 之间进行插值，以产生平滑的渐变效果。

**clip coordinate** 是 vertex shader 输出的四维向量，随后通过将整个向量除以其最后一个分量，转换为**normalized device coordinate（NDC）**。这些 NDC 是将 framebuffer 映射到 [-1, 1] × [-1, 1] 坐标系的齐次坐标，如下图所示：

![NDC 坐标系示意图](https://docs.vulkan.org/tutorial/latest/_images/images/normalized_device_coordinates.svg)

如果你之前接触过计算机图形学，应该对此很熟悉。如果你用过 OpenGL，你会注意到 Y 轴的符号现在是反转的，Z 轴使用与 Direct3D 相同的范围，即从 0 到 1。

对于我们的第一个三角形，我们不会应用任何变换，直接将三个顶点的位置以 NDC 形式指定，以创建如下形状：

![三角形顶点布局](https://docs.vulkan.org/tutorial/latest/_images/images/triangle_coordinates.svg)

我们可以通过将其作为 clip coordinate 从 vertex shader 输出来直接输出 NDC，令最后一个分量为 1，这样除法就不会改变任何值。

通常，这些坐标会存储在 vertex buffer 中，但在 Vulkan 中创建 vertex buffer 并填充数据并非易事。因此，我们决定暂时将其推迟，改为直接在 shader 中硬编码坐标。代码如下：

```slang
static float2 positions[3] = float2[](
    float2(0.0, -0.5),
    float2(0.5, 0.5),
    float2(-0.5, 0.5)
);

struct VertexOutput {
    float4 sv_position : SV_Position;
};

[shader("vertex")]
VertexOutput vertMain(uint vid : SV_VertexID) {
    VertexOutput output;
    output.sv_position = float4(positions[vid], 0.0, 1.0);
    return output;
}
```

`vertMain` 函数对每个顶点调用一次。参数中带有 `SV_VertexID` 注解的内置变量包含当前顶点的索引。它通常是 vertex buffer 的索引，但在我们的情况下，它是硬编码顶点数据数组的索引。从 `positions` 数组中查找每个顶点的位置，并将其与虚拟的 `z` 和 `w` 分量组合，以输出 clip coordinate 中的位置。

## Fragment shader

由 vertex shader 位置构成的三角形会用 fragment 填充屏幕上的某个区域。Fragment shader 对这些 fragment 调用，以产生颜色和深度值写入 framebuffer。为整个三角形输出红色的简单 fragment shader 如下：

```slang
[shader("fragment")]
float4 fragMain() : SV_Target
{
    return float4(1.0, 0.0, 0.0, 1.0);
}
```

`fragMain` 函数对每个 fragment 调用一次，用于为该 fragment 生成颜色。Slang 中的颜色是具有 R、G、B 和 alpha 通道的四分量向量，各通道值在 [0, 1] 范围内。与 vertex shader 中的 `sv_position` 不同，这里没有内置变量来输出当前 fragment 的颜色，你必须自己指定输出变量，返回类型上的 `SV_Target` 语义指定了这是颜色输出。`float4(1.0, 0.0, 0.0, 1.0)` 对应不透明红色。

## 每顶点颜色（Per-vertex colors）

将整个三角形显示为红色并不太有趣，下面这样的效果会更漂亮：

![彩色三角形](https://docs.vulkan.org/tutorial/latest/_images/images/triangle_coordinates_colors.svg)

为此，我们需要对两个 shader 都进行一些修改。首先，我们需要为三个顶点各指定一种不同的颜色。Vertex shader 现在应该包含一个颜色数组，就像位置数组那样：

```slang
static float3 colors[3] = float3[](
    float3(1.0, 0.0, 0.0),
    float3(0.0, 1.0, 0.0),
    float3(0.0, 0.0, 1.0)
);
```

现在我们需要将这些每顶点颜色传递给 fragment shader，以便它能将插值后的颜色输出到 framebuffer。在 `VertexOutput` struct 中添加一个 color 输出，并在 `vertMain` 函数中赋值：

```slang
struct VertexOutput {
    float3 color;
    float4 sv_position : SV_Position;
};

[shader("vertex")]
VertexOutput vertMain(uint vid : SV_VertexID) {
    VertexOutput output;
    output.sv_position = float4(positions[vid], 0.0, 1.0);
    output.color = colors[vid];
    return output;
}
```

接下来，我们需要修改 fragment shader 以接受来自 vertex shader 的颜色输入：

```slang
[shader("fragment")]
float4 fragMain(VertexOutput inVert) : SV_Target
{
    float3 color = inVert.color;
    return float4(color, 1.0);
}
```

`fragColor` 的值将在三个顶点之间的 fragment 上自动进行插值，从而产生平滑的渐变效果。

## 编译 shader（Compiling the shaders）

在项目的根目录中创建一个名为 `shaders` 的目录，并将 vertex shader 存储在名为 `shader.slang` 的文件中，将 fragment shader 存储在名为 `shader.frag` 的文件中（如果使用 GLSL）。Slang shader 没有官方的文件扩展名，但 `.slang` 是常用的约定。

`shader.slang` 的内容如下：

```slang
static float2 positions[3] = float2[](
    float2(0.0, -0.5),
    float2(0.5, 0.5),
    float2(-0.5, 0.5)
);

static float3 colors[3] = float3[](
    float3(1.0, 0.0, 0.0),
    float3(0.0, 1.0, 0.0),
    float3(0.0, 0.0, 1.0)
);

struct VertexOutput {
    float3 color;
    float4 sv_position : SV_Position;
};

[shader("vertex")]
VertexOutput vertMain(uint vid : SV_VertexID) {
    VertexOutput output;
    output.sv_position = float4(positions[vid], 0.0, 1.0);
    output.color = colors[vid];
    return output;
}

[shader("fragment")]
float4 fragMain(VertexOutput inVert) : SV_Target
{
    float3 color = inVert.color;
    return float4(color, 1.0);
}
```

现在，我们将使用 `slangc` 编译器将这些 shader 编译为 SPIR-V 字节码。

**Windows 系统（compile.bat）：**

```bat
C:/VulkanSDK/x.x.x.x/bin/slangc.exe shader.slang -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name -entry vertMain -entry fragMain -o slang.spv
```

**Linux 系统（compile.sh）：**

```bash
/home/user/VulkanSDK/x.x.x.x/x86_64/bin/slangc shader.slang -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name -entry vertMain -entry fragMain -o slang.spv
```

这两条命令告诉编译器读取 Slang 源文件，并使用 `-o`（输出）标志直接输出一个 SPIR-V 1.4 字节码文件。

如果你的 shader 包含语法错误，编译器会报告行号和具体问题，就像你期望的那样。你也可以尝试省略分号之类的操作来验证。还可以不带任何参数运行编译器，以查看它支持的标志类型。例如，它还可以将字节码输出为人类可读的格式，以便你准确了解 shader 正在做什么以及在此阶段应用了哪些优化。

在命令行上编译 shader 是最直接的选项之一，也是本教程将使用的方式，但也可以直接从代码中编译 shader。Vulkan SDK 包含 **libslang**，允许你在程序运行时从 Slang 代码编译 shader。

推荐使用 CMake 函数来编译 shader，作为最佳实践：

```cmake
function (add_slang_shader_target TARGET)
  cmake_parse_arguments ("SHADER" "" "" "SOURCES" ${ARGN})
  set (SHADERS_DIR ${CMAKE_CURRENT_LIST_DIR}/shaders)
  set (ENTRY_POINTS -entry vertMain -entry fragMain)
  add_custom_command (
          OUTPUT ${SHADERS_DIR}
          COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADERS_DIR}
  )
  add_custom_command (
          OUTPUT  ${SHADERS_DIR}/slang.spv
          COMMAND ${SLANGC_EXECUTABLE} ${SHADER_SOURCES} -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name ${ENTRY_POINTS} -o slang.spv
          WORKING_DIRECTORY ${SHADERS_DIR}
          DEPENDS ${SHADERS_DIR} ${SHADER_SOURCES}
          COMMENT "Compiling Slang Shaders"
          VERBATIM
  )
  add_custom_target (${TARGET} DEPENDS ${SHADERS_DIR}/slang.spv)
endfunction()
```

## 加载 shader（Loading a shader）

现在我们有了生成 SPIR-V shader 的方法，是时候将它们加载到程序中以便在某个时刻将其插入 graphics pipeline 了。我们首先编写一个简单的辅助函数来从文件中加载二进制数据。

```cpp
#include <fstream>

...

static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }
}
```

`readFile` 函数将从指定文件中读取所有字节，并以 `std::vector` 管理的字节数组形式返回。我们使用了两个标志来打开文件：

- `ate`：从文件末尾开始读取
- `binary`：以二进制文件形式读取（避免文本转换）

从文件末尾开始读取的优点是，我们可以使用读取位置来确定文件大小并分配缓冲区：

```cpp
std::vector<char> buffer(file.tellg());

file.seekg(0, std::ios::beg);
file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));

file.close();

return buffer;
```

现在从 `createGraphicsPipeline` 中调用此函数来加载这两个 shader 的字节码：

```cpp
void createGraphicsPipeline() {
    auto shaderCode = readFile("shaders/slang.spv");
}
```

通过打印 buffer 的大小并与实际文件大小进行对比，来确保 shader 加载正确。注意代码不需要以 null 结尾，因为它是二进制代码，我们稍后将明确指定其大小。

## 创建 shader module（Creating shader modules）

在将代码传递给 pipeline 之前，我们必须将其包装在一个 `vk::raii::ShaderModule` 对象中。让我们创建一个辅助函数 `createShaderModule` 来实现这一点。

```cpp
[[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) const
{
}
```

该函数接受一个字节码 buffer 作为参数，并以其为基础创建一个 `vk::raii::ShaderModule`。

创建 shader module 很简单，只需使用字节码及其长度指定一个指针即可。这些信息通过 `vk::ShaderModuleCreateInfo` 结构体指定。需要注意的一个细节是，字节码的大小以字节为单位指定，但字节码指针是 `uint32_t` 指针而非 `char` 指针。因此，我们需要使用 `reinterpret_cast` 来转换指针，如下所示。执行此类转换时，还需要确保数据满足 `uint32_t` 的对齐要求。对我们来说这很幸运，因为数据存储在 `std::vector` 中，其默认分配器已经确保数据满足最坏情况下的对齐要求。

```cpp
vk::ShaderModuleCreateInfo createInfo{
    .codeSize = code.size() * sizeof(char),
    .pCode = reinterpret_cast<const uint32_t*>(code.data())
};

vk::raii::ShaderModule shaderModule{ device, createInfo };

return shaderModule;
```

Shader module 只是对我们之前从文件加载的 shader 字节码及其中定义的函数的简单封装。在 GPU 上执行之前，SPIR-V 字节码到机器码的编译和链接是在 graphics pipeline 创建时完成的。这意味着一旦 pipeline 创建完成，我们就可以立即销毁 shader module，这也是为什么在 `createGraphicsPipeline` 函数中将它们声明为局部变量而非类成员：

```cpp
void createGraphicsPipeline() {
    vk::raii::ShaderModule shaderModule = createShaderModule(readFile("shaders/slang.spv"));
```

## Shader stage 创建（Shader stage creation）

要实际使用这些 shader，我们需要通过 `vk::PipelineShaderStageCreateInfo` 结构体将它们分配给特定的 pipeline stage，这是实际 pipeline 创建过程的一部分。

我们首先填写 vertex shader 的结构体：

```cpp
vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
    .stage = vk::ShaderStageFlagBits::eVertex,
    .module = shaderModule,
    .pName = "vertMain"
};
```

第一个参数告诉 Vulkan 该 shader 将在哪个 pipeline stage 使用。每个可编程阶段都有对应的枚举值，在上一章中有过介绍。

接下来的两个字段指定了包含代码的 shader module 以及要调用的函数，即**入口点（entrypoint）**。这意味着可以将多个 fragment shader 组合到一个 shader module 中，并通过不同的入口点来区分它们的行为。但在这里我们使用标准的 `vertMain` 和 `fragMain`。

还有一个可选的成员 `pSpecializationInfo`，虽然这里不使用，但值得一提。它允许你为 shader 常量指定值。你可以使用单个 shader module，通过为其中的常量指定不同值，在 pipeline 创建时配置出不同行为。这比在渲染时使用变量来配置 shader 更高效，因为编译器可以借此进行优化，例如消除依赖于这些值的 if 语句。如果没有这样的常量，只需将成员设置为 `nullptr`（结构体默认初始化即如此）。

修改结构体以适配 fragment shader：

```cpp
vk::PipelineShaderStageCreateInfo fragShaderStageInfo{
    .stage = vk::ShaderStageFlagBits::eFragment,
    .module = shaderModule,
    .pName = "fragMain"
};
```

最后，定义一个包含这两个结构体的数组，稍后在实际 pipeline 创建步骤中引用它：

```cpp
vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};
```

这就是关于如何描述 pipeline 可编程阶段的全部内容。在下一章中，我们将介绍固定功能阶段。

**[C++ 代码](https://docs.vulkan.org/tutorial/latest/_attachments/09_shader_modules.cpp)** | **[Slang shader](https://docs.vulkan.org/tutorial/latest/_attachments/09_shader_modules.slang)** | **[GLSL vertex shader](https://docs.vulkan.org/tutorial/latest/_attachments/09_shader_base.vert)** | **[GLSL fragment shader](https://docs.vulkan.org/tutorial/latest/_attachments/09_shader_base.frag)**
