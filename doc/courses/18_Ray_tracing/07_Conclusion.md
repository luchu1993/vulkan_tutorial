# 结语（Conclusion）

> 原文：https://docs.vulkan.org/tutorial/latest/courses/18_Ray_tracing/07_Conclusion.html

在本课程中，你已经使用 dynamic rendering 和 ray query 将光线追踪效果实现到了 Vulkan 光栅化管线中。让我们总结一下关键要点：

**Dynamic rendering**：简化了 render pass 的设置，现在是 Vulkan 中开始渲染的首选方式。我们通过 RenderDoc 验证了其使用情况。特别是对于移动端，结合本地 attachment 读取的扩展时，它是一个重大优势。

**Acceleration structures**：我们从加载的模型中创建了 BLAS 和 TLAS。使用 Nsight Graphics，我们确认这些结构已正确构建。

**Ray query 阴影**：我们在 fragment shader 中投射阴影光线。最初只考虑不透明几何体，然后我们通过手动检查交叉点的纹理 alpha 来处理 alpha 测试透明度。

**Ray query 反射**（额外功能）：我们投射反射光线，并使用命中结果调制反射材质的片元颜色。这利用了相同的加速结构，并使用了类似的 `Proceed()` 循环逻辑。你可以想象将其扩展到折射、环境光遮蔽光线等效果。

我们希望这个实验让你亲身体验了使用 Vulkan 最新功能的混合渲染。祝你在 Vulkan 中愉快地渲染，享受在应用程序中创建更高级的光线追踪效果！

---

## 参考资料（References）

- 在 https://github.com/KhronosGroup/Vulkan-Tutorial 上完成完整的 Vulkan Tutorial
- 在 https://www.khronos.org/vulkan 上查找更多 Vulkan 文档和资源
- 在 https://developer.arm.com/mobile-graphics-and-gaming/vulkan-api-best-practices-on-arm-gpus 阅读 Arm 的 Vulkan 最佳实践指南
- 在 https://github.com/baldurk/renderdoc 下载 RenderDoc
- 在 https://developer.nvidia.com/nsight-graphics 下载 NVIDIA Nsight Graphics
- 在 https://shader-slang.org 了解更多关于 Slang 着色语言的信息

3D 资产由 Poly Haven 提供并使用 Blender 合并：

- https://polyhaven.com/a/nettle_plant
- https://polyhaven.com/a/potted_plant_02
- https://polyhaven.com/a/wooden_picnic_table
