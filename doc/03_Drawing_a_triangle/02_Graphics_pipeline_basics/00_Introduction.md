# 图形管线简介（Introduction）

> 原文：[https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/02_Graphics_pipeline_basics/00_Introduction.html](https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/02_Graphics_pipeline_basics/00_Introduction.html)

在接下来的几章中，我们将设置一个 graphics pipeline，用于绘制我们的第一个三角形。graphics pipeline 是指将网格（mesh）的顶点（vertices）和纹理（textures）一路处理，最终写入 render target 中像素的一系列操作。其简化流程如下图所示：

![Vulkan 简化 pipeline 示意图](https://docs.vulkan.org/tutorial/latest/_images/images/vulkan_simplified_pipeline.svg)

**Input assembler** 从你指定的 buffer 中收集原始顶点数据，也可以使用 index buffer 来重复使用某些元素，而无需重复存储顶点数据本身。

**Vertex shader** 对每个顶点运行一次，通常用于将顶点位置从模型空间（model space）变换到屏幕空间（screen space）。它还会将每个顶点的数据传递给后续管线阶段。

**Tessellation shader** 允许你根据特定规则对几何体进行细分，以提升网格质量。常见用途是使砖墙、楼梯等表面在近距离观察时看起来不那么平坦。

**Geometry shader** 对每个图元（primitive，如三角形、线段、点）运行一次，可以丢弃该图元，也可以输出比输入更多的图元。这与 tessellation shader 类似，但更加灵活。不过在当今的应用中，它的使用并不多，因为其在除 Intel 集成显卡以外的大多数显卡上性能表现不佳。mesh shader 在某些方面可以更好地替代它。

**Rasterization** 阶段将图元离散化为 fragment。这些 fragment 即为 fragment shader 所填充的像素元素。位于屏幕之外的 fragment 会被裁剪，vertex shader 输出的顶点属性也会在各个 fragment 之间进行插值（如下图所示）。由于深度测试（depth testing）的存在，位于其他图元 fragment 后方的 fragment 通常也会在此阶段被丢弃。

**Fragment shader** 对每个存活的 fragment 调用，决定该 fragment 将被写入哪个（或哪些）framebuffer，以及写入什么颜色和深度值。它可以使用来自 vertex shader 的插值数据，包括纹理坐标和法线用于光照计算等。

**Color blending** 阶段对映射到 framebuffer 中同一像素位置的不同 fragment 应用混合操作。Fragment 可以直接覆盖彼此、叠加，也可以根据透明度进行混合。

绿色阶段在上图中被称为**固定功能（fixed-function）**阶段。这些阶段允许你通过参数来调整其行为，但其工作方式是预定义的。

橙色阶段是**可编程（programmable）**的，这意味着你可以将自己的代码上传到显卡，以精确地应用你想要的操作。例如，这使得 fragment shader 可以实现从纹理贴图到光线追踪的任何功能。这些程序在众多 GPU 核心上同时运行，以并行处理多个顶点和 fragment 等对象。

如果你以前使用过 OpenGL 和 Direct3D 等旧版 API，你可能习惯于通过 `glBlendFunc` 和 `OMSetBlendState` 等调用来随意修改任何 pipeline 设置。Vulkan 中的 graphics pipeline **几乎完全不可变（almost completely immutable）**，因此如果你想更改 shader、绑定不同的 framebuffer 或修改混合函数，就必须从头重建 pipeline。这样做的缺点是，你需要为渲染操作的所有不同组合创建多个 pipeline。然而，由于你在 pipeline 中执行的所有操作都是事先已知的，驱动程序可以对其进行更好地优化。

某些 pipeline 阶段是可选的，例如，如果你只是绘制简单的几何体，可以禁用 tessellation 和 geometry shader 阶段。

在接下来的章节中，我们将首先创建两个必不可少的阶段：vertex shader 和 fragment shader，用于在屏幕上绘制一个三角形。固定功能配置（如混合模式、viewport、rasterization）将在之后设置。在 Vulkan 中设置 graphics pipeline 的最后一步是指定输入和输出的 framebuffer。

创建一个 `createGraphicsPipeline` 函数，在 `initVulkan` 中的 `createImageViews` 调用之后调用它。我们将在接下来的章节中逐步完善这个函数。

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
}

...

void createGraphicsPipeline() {

}
```

**[C++ 代码](https://docs.vulkan.org/tutorial/latest/_attachments/08_graphics_pipeline.cpp)**
