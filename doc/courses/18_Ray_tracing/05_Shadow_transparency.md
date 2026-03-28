# 阴影透明度（Shadow Transparency）

> 原文：https://docs.vulkan.org/tutorial/latest/courses/18_Ray_tracing/05_Shadow_transparency.html

## 目标（Objective）

为场景中的透明物体添加支持。我们将实现一个简单的 alpha 测试来丢弃 alpha 值较低的片元，并在对应的光线追踪阴影中复现这一效果。

到目前为止，我们将所有几何体都视为不透明的。然而，叶片是使用 alpha 通道定义透明度的纹理渲染的，目前这一点没有被考虑进去，因此在叶片边缘会出现一些暗色像素。

---

## Task 7：Alpha 截断透明度（Alpha-cut transparency）

要实现 alpha 截断透明度，我们将在 fragment shader 中丢弃 alpha 值较低的片元。这是一种常见技术，用于处理透明纹理而无需复杂的混合或排序：

```slang
   float4 baseColor = textures[pc.materialIndex].Sample(textureSampler, vertIn.fragTexCoord);
   // Alpha test
   if (baseColor.a < 0.5) discard;
```

注意阴影暂时保持不变。我们首先需要做的是，只有当 BLAS 没有 alpha 透明度时才使用 `eOpaque` 标志，在 `createAccelerationStructures()` 中：

```cpp
vk::AccelerationStructureGeometryKHR blasGeometry{
    .geometryType = vk::GeometryTypeKHR::eTriangles,
    .geometry = geometryData
};
blasGeometry.flags = (submesh.alphaCut) ? vk::GeometryFlagsKHR(0) : vk::GeometryFlagBitsKHR::eOpaque;
```

使用以下设置重新构建并运行：

```cpp
#define LAB_TASK_LEVEL 7
```

你会发现叶片的阴影现在完全消失了！这是因为在所有交叉候选中，只有不透明几何体的三角形会被自动提交。我们需要实现一种在 ray query 阴影中处理透明度的方式，并有条件地提交候选交叉点。

在此之前，让我们先用 Nsight Graphics 检查我们迄今为止构建的加速结构，以了解它们的结构以及每个三角形可用的信息。

![alphacut before](https://docs.vulkan.org/tutorial/latest/_images/images/38_TASK08_alphacut_before.png)

---

## Task 8：使用 NVIDIA Nsight Graphics 检查加速结构（Inspect the acceleration structures with NVIDIA Nsight Graphics）

RenderDoc 目前还不支持检查 Vulkan 加速结构，但我们可以使用 Nsight Graphics 来验证我们的 BLAS/TLAS 是否正确构建。这有助于在着色器依赖它们之前捕获错误（例如几何体数量错误、偏移量错误）。

使用 Nsight：

1. 指定可执行文件路径：`C:\Users\nvidia\S2025_LABS\gensubcur_105\Vulkan-Tutorial\attachments\build\38_ray_tracing\Debug\38_ray_tracing.exe`
2. 指定工作目录：`C:\Users\nvidia\S2025_LABS\gensubcur_105\Vulkan-Tutorial\attachments\build\38_ray_tracing`
3. 启动应用程序

然后捕获一帧并点击"Start Graphics Debugger"：

![Nsight launch](https://docs.vulkan.org/tutorial/latest/_images/images/38_TASK04_nsight_launch.png)

与 RenderDoc 类似，你可以检查事件并找到加速结构构建命令，这将带我们到加速结构可视化工具：

![Nsight capture](https://docs.vulkan.org/tutorial/latest/_images/images/38_TASK04_nsight_capture.png)

- 使用 Event Browser 中的搜索功能过滤并找到 draw call
- 选择任意一个 draw call，这将打开 API Inspector
- 选择 FS 以找到绑定到 fragment shader 的资源
- 点击加速结构绑定（这是我们的 TLAS），打开"Ray Tracing Inspector"

在新窗口中，你可以看到 TLAS 及其实例：

![Nsight main](https://docs.vulkan.org/tutorial/latest/_images/images/38_TASK04_nsight_main.png)

你可以展开每个实例以查看它引用的 BLAS，并检查几何体数据。

- 你可能会觉得"Orbit Camera"在场景导航中更舒适
- 你可能需要将"Up Direction"设置为"Z Axis"以匹配模型使用的坐标系
- 使用"Opaque"几何体的过滤器（预期中，叶片模型将显示为灰色）
- 尝试不同的可视化选项，如"Color by"
- 尝试"Show Mesh Wireframes"和"Show AABB Wireframes"

![Nsight inspector](https://docs.vulkan.org/tutorial/latest/_images/images/38_TASK04_nsight_inspector.png)

---

## Task 9：无绑定资源与实例查找表（Bindless resources and instance look-up table）

到目前为止，我们不需要知道光线命中了哪个三角形，我们只关心它是否命中了任何东西。但为了实现透明度，我们需要知道用于着色命中点的纹理的 alpha 值。这样我们可以确定是否命中了透明像素，如果是，则需要继续遍历，以防在通往光源的路上后面有其他不透明三角形。

在引入所需的 ray query 逻辑之前，让我们先观察渲染器如何在着色器中绑定我们需要的所有内容，并实现一个简单的查找表，将加速结构实例映射到其几何体和纹理数据。

注意渲染器并没有为每个子网格分别绑定材质纹理。相反，它绑定了一个纹理数组，并使用材质索引来查找每个子网格的纹理。我们使用 push constant 来传递材质索引：

```cpp
        for (auto& sub : submeshes) {
            // TASK09: Bindless resources
            PushConstant pushConstant = {
                .materialIndex = sub.materialID < 0 ? 0u : static_cast<uint32_t>(sub.materialID),
            };
            commandBuffers[frameIndex].pushConstants<PushConstant>(pipelineLayout, vk::ShaderStageFlagBits::eFragment, 0, pushConstant);
            commandBuffers[frameIndex].drawIndexed(sub.indexCount, 1, sub.indexOffset, 0, 0);
        }
```

然后，在着色器中，我们使用材质索引来采样纹理数组。它们都共享相同的采样器：

```slang
[[vk::binding(0,1)]]
SamplerState textureSampler;
[[vk::binding(1,1)]]
Texture2D<float4> textures[];
struct PushConstant {
    uint materialIndex;
};
[push_constant]
PushConstant pc;
[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
   float4 baseColor = textures[pc.materialIndex].Sample(textureSampler, vertIn.fragTexCoord);
```

这是一种称为"无绑定资源"（bindless resources）的常见技术，它允许我们减少所需的 descriptor set 和 binding 数量，并更容易管理拥有多个对象的场景中的材质。它需要 descriptor indexing 扩展，该扩展从 Vulkan 1.2 起已成为核心。

我们不能在光线遍历中使用 push constant，因为光线可能会命中场景中的任何几何体，而不仅仅是我们当前正在着色的那个。但是，我们可以用自定义索引标记每个加速结构实例，然后使用此索引和查找表（LUT）找到命中实例的几何体和纹理。

在 `createAccelerationStructures()` 中，当我们遍历模型子网格时，我们需要在 `AccelerationStructureInstanceKHR` 结构体中添加一个新字段，为每个子网格保存一个唯一索引：

```cpp
vk::AccelerationStructureInstanceKHR instance{
    .transform = identity,
    .mask = 0xFF,
    .accelerationStructureReference = blasDeviceAddr
};
instances.push_back(instance);
instances[i].instanceCustomIndex = static_cast<uint32_t>(i);
```

如果你现在运行应用程序并用 Nsight Graphics 捕获，你将能够按"Instance Custom Index"着色，以查看分配给每个实例的索引，而之前它们都是相同的：

![instance LUT](https://docs.vulkan.org/tutorial/latest/_images/images/38_TASK09_instance_lut.png)

然后，填充一个 LUT 条目向量。使用相同的子网格索引，我们需要为每个子网格存储材质 ID 和索引 buffer 偏移量：

```cpp
// TASK09: store the instance look-up table entry
instanceLUTs.push_back({ static_cast<uint32_t>(submesh.materialID), submesh.indexOffset });
```

与创建 LUT buffer 相关的其余代码可以在 `createDescriptorSets()` 和 `createInstanceLUTBuffer()` 中找到。注意着色器中已经定义了相应的 binding：

```slang
// TASK09: Instance look-up table
struct InstanceLUT {
    uint materialID;
    uint indexBufferOffset;
};
[[vk::binding(4,0)]]
StructuredBuffer<InstanceLUT> instanceLUTBuffer;
```

现在我们将看到如何将这些资源与 ray query 结合起来处理透明交叉点。

![instance custom index](https://docs.vulkan.org/tutorial/latest/_images/images/38_TASK09_instance_custom_index.png)

---

## Task 10：带 Alpha 测试的 Ray Query（Ray query with alpha test）

还记得 `Proceed()` 如何将 `RayQuery` 对象的状态推进到光线沿途的下一个交叉候选吗？这就是我们实现 alpha 测试逻辑的地方。我们将检查用于着色命中三角形的纹理的 alpha 值，如果低于某个阈值，我们将继续遍历以找到下一个不透明三角形。一旦找到，我们就"提交"它，遍历将结束。

首先，用一个循环替换该调用，并从候选命中中检索必要的属性。然后我们将把这些传递给辅助函数 `intersection_uv`，该函数将检索我们在三角形内命中点的纹理坐标：

```slang
    while (sq.Proceed())
    {
        uint instanceID = sq.CandidateRayInstanceCustomIndex();
        uint primIndex = sq.CandidatePrimitiveIndex();
        float2 uv = intersection_uv(instanceID, primIndex, sq.CandidateTriangleBarycentrics());
    }
```

以下是此辅助函数的定义：

```slang
float2 intersection_uv(uint instanceID, uint primIndex, float2 barycentrics) {
    uint indexOffset = instanceLUTBuffer[NonUniformResourceIndex(instanceID)].indexBufferOffset;
    uint i0 = indexBuffer[indexOffset + (primIndex * 3 + 0)];
    uint i1 = indexBuffer[indexOffset + (primIndex * 3 + 1)];
    uint i2 = indexBuffer[indexOffset + (primIndex * 3 + 2)];
    float2 uv0 = uvBuffer[i0];
    float2 uv1 = uvBuffer[i1];
    float2 uv2 = uvBuffer[i2];
    float w0 = 1.0 - barycentrics.x - barycentrics.y;
    float w1 = barycentrics.x;
    float w2 = barycentrics.y;
    return w0 * uv0 + w1 * uv1 + w2 * uv2;
}
```

- `instanceID` 允许我们从实例 LUT 中检索 `indexBufferOffset` 和 `materialID`
- `indexBufferOffset` 用于找到实例的索引 buffer。注意索引 buffer 包含场景中所有模型的索引，因此我们需要将其缩小到命中的模型（例如叶片）
- `primIndex` 是实例索引 buffer 部分中三角形的索引
- `NonUniformResourceIndex()` 表示资源索引可能在单个 draw 或 dispatch 调用中的不同着色器调用之间不同，防止不必要的编译器优化

一旦我们将命中缩小到模型中的特定三角形，就可以在 `uvBuffer` 中检索其纹理坐标，该 buffer 包含场景中所有顶点的 UV 坐标。

最后，它根据交叉点的重心坐标插值命中三角形的纹理坐标。

然后我们可以使用这些 UV 坐标采样纹理并检索 alpha 值：

```slang
        uint materialID = instanceLUTBuffer[NonUniformResourceIndex(instanceID)].materialID;
        float4 intersection_color = textures[NonUniformResourceIndex(materialID)].SampleLevel(textureSampler, uv, 0);
```

根据 alpha 值，我们可以决定是继续追踪还是提交命中：

```slang
        if (intersection_color.a < 0.5) {
            // If the triangle is transparent, we continue to trace
            // to find the next opaque triangle.
        } else {
            // If we hit an opaque triangle, we stop tracing.
            sq.CommitNonOpaqueTriangleHit();
        }
```

完整的 `Proceed()` 循环应该如下所示：

```slang
    while (sq.Proceed())
    {
        uint instanceID = sq.CandidateRayInstanceCustomIndex();
        uint primIndex = sq.CandidatePrimitiveIndex();
        float2 uv = intersection_uv(instanceID, primIndex, sq.CandidateTriangleBarycentrics());
        uint materialID = instanceLUTBuffer[NonUniformResourceIndex(instanceID)].materialID;
        float4 intersection_color = textures[NonUniformResourceIndex(materialID)].SampleLevel(textureSampler, uv, 0);
        if (intersection_color.a < 0.5) {
            // If the triangle is transparent, we continue to trace
            // to find the next opaque triangle.
        } else {
            // If we hit an opaque triangle, we stop tracing.
            sq.CommitNonOpaqueTriangleHit();
        }
    }
```

注意不透明命中会自动提交，永远不会进入循环。

使用以下设置重新构建并运行：

```cpp
#define LAB_TASK_LEVEL 10
```

此时，你已经通过 ray query 实现了支持透明度的健壮阴影！这是一个重要功能，对于精细的 alpha 细节来说，传统的阴影贴图很难实现：

![alphacut after](https://docs.vulkan.org/tutorial/latest/_images/images/38_TASK08_alphacut_after.png)

有了支持阴影透明度所需的一切，实现其他效果（如反射）就非常简单了！

![alphacut shadows](https://docs.vulkan.org/tutorial/latest/_images/images/38_TASK10_alphacut_shadows.png)
