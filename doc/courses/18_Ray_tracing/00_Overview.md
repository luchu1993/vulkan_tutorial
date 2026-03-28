# 概述（Overview）

> 原文：https://docs.vulkan.org/tutorial/latest/courses/18_Ray_tracing/00_Overview.html

## 简介（Introduction）

欢迎！在本系列中，我们将为一个 Vulkan 渲染器添加光线追踪（ray tracing）功能，以实现实时像素级精确的阴影（支持和不支持透明度）以及一个额外的反射效果。你将使用提供的脚手架代码（基于 Vulkan Tutorial）并按照逐步说明填入关键的着色器函数。

幻灯片可在此处获取。

学完本系列后，你将学会：

- 使用 Vulkan dynamic rendering（无需预定义 render pass）并用 RenderDoc 验证。
- 为光线追踪创建 Bottom-Level 和 Top-Level Acceleration Structures（BLAS 和 TLAS）。
- 实现基于 ray query 的阴影光线（先对完全不透明的几何体，再支持 alpha 测试的透明度）。
- 在 Nsight Graphics 中调试/检查加速结构。
- （额外）实现基于 ray query 的反射。

前提条件：

本系列面向中级 Vulkan 程序员。我们假设你已完成基础 Vulkan Tutorial（图形管线、descriptor set 等）。即使没有完成，你仍可以从概念层面跟随学习。需要一台安装了最新 Vulkan SDK（1.4.311）、支持光线追踪（Vulkan Ray Query）的 GPU 的 Windows 机器，以及 RenderDoc 和 Nsight Graphics。

移动端最佳实践说明：

我们使用 `VK_KHR_dynamic_rendering`（Vulkan 1.3 的核心功能）而非传统 render pass。这不仅简化了 API，而且通过 `VK_KHR_dynamic_rendering_local_read` 扩展，能够实现类似 subpass 的 tile 本地存储读取，对于移动端基于 tile 的 GPU 来说意义重大，可以节省带宽。

我们在 fragment shader 中使用 Ray Query（来自 `VK_KHR_ray_query`），而非独立的光线追踪管线。在移动端，ray query 的支持范围更广，并且（取决于使用场景）通常比完整的光线追踪管线更高效。Ray query 可以很好地集成到 fragment shading 中，受益于片上压缩并避免上下文切换。

---

## 提供的代码（Provided Code）

提供的代码是一个已经实现了基于 dynamic rendering 的基础图形管线的 Vulkan 渲染器。它基于 Vulkan Tutorial，所有内容自包含在一个 C++ 源文件和一个 Slang 着色器文件中：

- `38_ray_tracing.cpp`
- `38_ray_tracing.slang`

如果你在着色器任务上遇到困难，可以参考提供的参考解决方案：`38_ray_tracing_complete.slang`

源代码的结构旨在引导你完成各个步骤，并提供了提示和样板代码。它还设置了所有必需的扩展和功能，包括 `VK_KHR_acceleration_structure` 和 `VK_KHR_ray_query`。

你将在代码中找到标有 `// TASKxx` 的代码段，我们在每个章节中都会引用这些内容。

在 C++ 文件的顶部，注意以下变量：

```cpp
#define LAB_TASK_LEVEL 1
```

在某些节点，你将被指示更新此变量并重新构建以验证关键更改的效果，无需编写新代码。

---

## 本系列的章节（Chapters in this series）

1. 概述（Overview）
2. Dynamic rendering
3. Acceleration structures
4. Ray query shadows
5. TLAS animation
6. Shadow transparency
7. Reflections
8. Conclusion

---

## 构建与运行（Build and run）

你可以从 Visual Studio 构建并运行提供的示例（将 `38_ray_tracing` 设置为启动项目），或通过命令行：

```
cmake --build build --target 38_ray_tracing --parallel; start .\build\38_ray_tracing\Debug\38_ray_tracing.exe -wo .\build\38_ray_tracing\
```

注意，以上内容与基础教程没有变化，你可以像在教程其余部分中一样在其他平台上继续构建。
