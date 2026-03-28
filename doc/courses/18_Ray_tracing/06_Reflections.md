# Ray Query 反射（Ray Query Reflections）

> 原文：https://docs.vulkan.org/tutorial/latest/courses/18_Ray_tracing/06_Reflections.html

## 目标（Objective）

我们将从片元沿镜面反射方向投射一条光线，以查看它命中什么，从而模拟反射材质（如镜子或光亮表面）。

反射的实现方式与阴影光线类似，但我们从着色点沿镜面方向投射一条光线，并采样命中表面的颜色。

---

## Task 11：实现 Ray Query 反射（Implement ray query reflections）

首先，我们将使用 push constant 将反射材质标志传递给 fragment shader。这将允许我们确定当前材质是否具有反射性。

假设今天下了雨，桌子上有水，因此它会反射环境。

我们需要更新 `PushConstant` 结构体以包含反射标志，在渲染器中：

```cpp
struct PushConstant {
    uint32_t materialIndex;
    uint32_t reflective;
};
```

以及在着色器中：

```slang
struct PushConstant {
    uint materialIndex;
    uint reflective;
};
[push_constant]
PushConstant pc;
```

并更新我们在发出 draw call 之前分配给它的值：

```cpp
PushConstant pushConstant = {
    .materialIndex = sub.materialID < 0 ? 0u : static_cast<uint32_t>(sub.materialID),
    .reflective = sub.reflective
};
commandBuffers[frameIndex].pushConstants<PushConstant>(pipelineLayout, vk::ShaderStageFlagBits::eFragment, 0, pushConstant);
commandBuffers[frameIndex].drawIndexed(sub.indexCount, 1, sub.indexOffset, 0, 0);
```

然后，我们将在 fragment shader 中检索这个值，在应用阴影效果之前，调用一个辅助函数，该函数将根据反射 ray query 就地修改片元颜色：

```slang
   float3 P = vertIn.worldPos;
   float3 N = vertIn.fragNormal;
   if (pc.reflective > 0) {
       apply_reflection(P, N, baseColor);
   }
   bool inShadow = in_shadow(P);
```

`apply_reflection()` 函数的实现与 `in_shadow()` 函数类似。`Proceed()` 循环不再是可选的，因为我们不仅需要检查是否有任何交叉点，我们还需要命中三角形的完整颜色来应用反射效果。

注意它需要法线方向（N）。这是因为反射是表面法线和视图方向 V 的函数。反射方向 R 可以使用内置的 `reflect()` 函数轻松计算：

```slang
void apply_reflection(float3 P, float3 N, inout float4 baseColor) {
    // Build the reflections ray
    float3 V = normalize(ubo.cameraPos - P);
    float3 R = reflect(-V, N);
```

然后定义光线描述，与阴影的方式类似：

```slang
    RayDesc reflectionRayDesc;
    reflectionRayDesc.Origin = P;
    reflectionRayDesc.Direction = R;
    reflectionRayDesc.TMin = EPSILON;
    reflectionRayDesc.TMax = 1e4;
```

并初始化 `RayQuery` 对象。但在这种情况下，我们不能使用 `RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH` 标志，因为我们需要检索最近三角形的完整颜色，而不仅仅是任意三角形：

```slang
    // Initialize a ray query for reflections
    RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> rq;
    let rayFlags = RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES;
```

现在我们可以发射我们的反射光线：

```slang
    rq.TraceRayInline(accelerationStructure, rayFlags, 0xFF, reflectionRayDesc);
```

`Proceed()` 循环完全相同：

```slang
    while (rq.Proceed())
    {
        uint instanceID = rq.CandidateRayInstanceCustomIndex();
        uint primIndex = rq.CandidatePrimitiveIndex();
        float2 uv = intersection_uv(instanceID, primIndex, rq.CandidateTriangleBarycentrics());
        uint materialID = instanceLUTBuffer[NonUniformResourceIndex(instanceID)].materialID;
        float4 intersection_color = textures[NonUniformResourceIndex(materialID)].SampleLevel(textureSampler, uv, 0);
        if (intersection_color.a < 0.5) {
            // If the triangle is transparent, we continue to trace
            // to find the next opaque triangle.
        } else {
            // If we hit an opaque triangle, we stop tracing.
            rq.CommitNonOpaqueTriangleHit();
        }
    }
    bool hit = (rq.CommittedStatus() == COMMITTED_TRIANGLE_HIT);
```

我们唯一需要的额外逻辑是检索命中三角形的颜色并将其应用到片元的基础颜色上。注意这里的逻辑与循环中几乎相同，但这次我们使用 `Committed` 版本的函数，而不是 `Candidate`：

```slang
    if (hit)
    {
        uint instanceID = rq.CommittedRayInstanceCustomIndex();
        uint primIndex = rq.CommittedPrimitiveIndex();
        float2 uv = intersection_uv(instanceID, primIndex, rq.CommittedTriangleBarycentrics());
        uint materialID = instanceLUTBuffer[NonUniformResourceIndex(instanceID)].materialID;
        float4 intersectionColor = textures[NonUniformResourceIndex(materialID)].SampleLevel(textureSampler, uv, 0);
        baseColor.rgb = lerp(baseColor.rgb, intersectionColor.rgb, 0.7);
    }
```

作为练习，你可以扩展这个函数，在光线未命中场景中任何几何体且没有提交的三角形命中时采样天空盒。

使用以下设置重新构建并运行：

```cpp
#define LAB_TASK_LEVEL 11
```

完成这一切后，你现在应该能在桌子上看到一些闪亮的反射效果：

![concept reflections](https://docs.vulkan.org/tutorial/latest/_images/images/38_TASK11_concept_reflections.png)

![alphacut reflections](https://docs.vulkan.org/tutorial/latest/_images/images/38_TASK11_alphacut_reflections.png)
