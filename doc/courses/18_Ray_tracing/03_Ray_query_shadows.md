# Ray Query 阴影（Ray Query Shadows）

> 原文：https://docs.vulkan.org/tutorial/latest/courses/18_Ray_tracing/03_Ray_query_shadows.html

## 目标（Objective）

在 fragment shader 中使用 ray query 添加一个简单的阴影测试。我们将从每个片元位置向光源投射一条光线，如果有物体被击中，则使片元变暗（硬阴影）。

恭喜：你已经为场景建立了有效的 TLAS/BLAS！现在，让我们用它来投射一些光线。

---

## Task 5：实现 Ray Query 阴影（Implement ray query shadows）

在 fragment shader 中，我们将使用 ray query 从片元位置向光源投射阴影光线。如果光线在到达光源之前击中任何几何体，我们将使片元颜色变暗：

```slang
// TASK05: Implement ray query shadows
bool in_shadow(float3 P)
{
    bool hit = false;
    return hit;
}
[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
   float4 baseColor = textures[pc.materialIndex].Sample(textureSampler, vertIn.fragTexCoord);
   float3 P = vertIn.worldPos;
   bool inShadow = in_shadow(P);
   // Darken if in shadow
   if (inShadow) {
       baseColor.rgb *= 0.2;
   }
   return baseColor;
}
```

为此，你将实现一个辅助函数 `in_shadow()`，它执行 ray query。首先定义一个光线描述，并用片元位置和光线方向初始化它：

```slang
bool in_shadow(float3 P)
{
    // Build the shadow ray from the world position toward the light
    RayDesc shadowRayDesc;
    shadowRayDesc.Origin = P;
    shadowRayDesc.Direction = normalize(lightDir);
    shadowRayDesc.TMin = EPSILON;
    shadowRayDesc.TMax = 1e4;
```

`TMin` 和 `TMax` 定义了光线从其原点出发的最小和最大距离。`EPSILON` 是一个小值，用于避免自相交；`1e4` 是一个大值，确保我们能够击中远处的物体。

接下来，我们将初始化一个 `RayQuery` 对象，用于执行光线遍历。注意我们使用的标志以提高速度：

- `RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES`：因为这是一个只有三角形的简单场景。
- `RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH`：一旦找到第一个不透明的交点就结束遍历，这对于阴影测试已经足够，因为我们只需要知道是否有任何东西遮挡了光线。

```slang
    // Initialize a ray query for shadows
    RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES |
             RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> sq;
    let rayFlags = RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES |
             RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;
```

然后，我们将启动光线追踪操作，它将我们的光线描述、`RayQuery` 对象和加速结构结合在一起：

```slang
    sq.TraceRayInline(accelerationStructure, rayFlags, 0xFF, shadowRayDesc);
    sq.Proceed();
```

`Proceed()` 将 `RayQuery` 对象的状态推进到光线沿途的下一个交叉"候选"。每次调用 `Proceed()` 都会检查是否还有另一个交点需要处理。如果有，它会更新查询的内部状态，让你可以访问当前候选交点的信息。这允许你为处理交点实现自定义逻辑，例如跳过透明表面（我们稍后会在实验中重新讨论）或在第一个不透明命中处停止。它通常在循环中调用以迭代所有潜在的交点，但对于阴影，我们只需要第一个命中：

```slang
    // If the shadow ray intersects an opaque triangle, we consider the pixel in shadow
    bool hit = (sq.CommittedStatus() == COMMITTED_TRIANGLE_HIT);
    return hit;
}
```

就是这样！你已经使用 ray query 实现了一个基本的阴影测试。如果光线在到达光源之前击中任何几何体，`in_shadow()` 函数将返回 `true`，表示该片元处于阴影中。

使用以下设置重新构建并运行：

```cpp
#define LAB_TASK_LEVEL 5
```

你会注意到有些地方不对：

![concept shadows](https://docs.vulkan.org/tutorial/latest/_images/images/38_TASK06_concept_shadows.png)

物体在旋转，但阴影是静态的。这是因为我们尚未更新 TLAS 以考虑物体的动画。每当物体移动或动画时，TLAS 都需要重建，所以接下来让我们实现它。

![shadows static](https://docs.vulkan.org/tutorial/latest/_images/images/38_TASK06_shadows_static.gif)
