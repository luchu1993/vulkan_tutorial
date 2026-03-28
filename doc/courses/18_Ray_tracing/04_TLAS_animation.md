# TLAS 动画（TLAS Animation）

> 原文：https://docs.vulkan.org/tutorial/latest/courses/18_Ray_tracing/04_TLAS_animation.html

## 目标（Objective）

通过每帧使用更新后的实例变换重建 TLAS，确保当物体动画时阴影也随之更新。

为了考虑物体的动画，我们需要在物体移动或发生变化时更新 TLAS。这涉及更新实例变换并重建 TLAS。我们将在 `updateTopLevelAS()` 函数中完成这一操作，该函数每帧使用当前的模型矩阵调用。

---

## Task 6：为动画更新 TLAS（Update the TLAS for animations）

首先，我们需要使用当前的模型矩阵更新实例变换。这通过遍历 `instances` 向量并为每个实例设置变换，然后更新实例 buffer 来完成。

```cpp
vk::TransformMatrixKHR tm{};
auto &M = model;
tm.matrix = std::array<std::array<float,4>,3>{{
        std::array<float,4>{M[0][0], M[1][0], M[2][0], M[3][0]},
        std::array<float,4>{M[0][1], M[1][1], M[2][1], M[3][1]},
        std::array<float,4>{M[0][2], M[1][2], M[2][2], M[3][2]}
}};
// TASK06: update the instances to use the new transform matrix.
for (auto & instance : instances) {
        instance.setTransform(tm);
}
```

接下来，我们需要为 TLAS 构建准备几何体数据。这与我们创建 TLAS 时的操作类似，但现在我们将使用更新后的实例 buffer。我们还需要将构建模式改为 `eUpdate`，并定义源 TLAS 和目标 TLAS。这指示实现就地更新现有 TLAS，而不是创建一个新的。当只有微小的变化（如变换）发生时，这样更高效：

```cpp
        // Prepare the geometry (instance) data
        auto instancesData = vk::AccelerationStructureGeometryInstancesDataKHR{
            .arrayOfPointers = vk::False,
            .data = instanceAddr
        };
        vk::AccelerationStructureGeometryDataKHR geometryData(instancesData);
        vk::AccelerationStructureGeometryKHR tlasGeometry{
            .geometryType = vk::GeometryTypeKHR::eInstances,
            .geometry = geometryData
        };
        // TASK06: Note the new parameters to re-build the TLAS in-place
        vk::AccelerationStructureBuildGeometryInfoKHR tlasBuildGeometryInfo{
            .type = vk::AccelerationStructureTypeKHR::eTopLevel,
            .flags = vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate,
            .mode = vk::BuildAccelerationStructureModeKHR::eUpdate,
            .srcAccelerationStructure = tlas,
            .dstAccelerationStructure = tlas,
            .geometryCount = 1,
            .pGeometries = &tlasGeometry
        };
        vk::BufferDeviceAddressInfo scratchAddressInfo{ .buffer = *tlasScratchBuffer };
        vk::DeviceAddress scratchAddr = device.getBufferAddressKHR(scratchAddressInfo);
        tlasBuildGeometryInfo.scratchData.deviceAddress = scratchAddr;
```

我们可以继续重用相同的 scratch buffer。注意还需要一个额外的实现提示，即 `eAllowUpdate` 标志，以指定我们打算更新此 TLAS。我们还需要回到 `createAccelerationStructures()` 函数，在第一次创建 TLAS 时添加此标志：

```cpp
        vk::AccelerationStructureBuildGeometryInfoKHR tlasBuildGeometryInfo{
            .type = vk::AccelerationStructureTypeKHR::eTopLevel,
            .flags = vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate, // <---- TASK06
            .mode = vk::BuildAccelerationStructureModeKHR::eBuild,
            .geometryCount = 1,
            .pGeometries = &tlasGeometry
        };
```

接下来，我们需要为 TLAS 准备构建 range，这与创建 TLAS 时的操作类似：

```cpp
        // Prepare the build range for the TLAS
        vk::AccelerationStructureBuildRangeInfoKHR tlasRangeInfo{
            .primitiveCount = primitiveCount,
            .primitiveOffset = 0,
            .firstVertex = 0,
            .transformOffset = 0
        };
```

最后，我们可以发出命令重建 TLAS。这里需要一个主要变化，关于同步。由于我们每帧都调用 `updateTopLevelAS()`，我们需要一个预构建内存 barrier 来确保所有对加速结构传输的写入，或前一帧着色器的读取，在构建开始之前完成：

```cpp
        // Re-build the TLAS
        auto cmd = beginSingleTimeCommands();
        // Pre-build barrier
        vk::MemoryBarrier preBarrier {
            .srcAccessMask = vk::AccessFlagBits::eAccelerationStructureWriteKHR | vk::AccessFlagBits::eTransferWrite | vk::AccessFlagBits::eShaderRead,
            .dstAccessMask = vk::AccessFlagBits::eAccelerationStructureReadKHR | vk::AccessFlagBits::eAccelerationStructureWriteKHR
        };
        cmd->pipelineBarrier(
            vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR | vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eFragmentShader, // srcStageMask
            vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, // dstStageMask
            {}, // dependencyFlags
            preBarrier, // memoryBarriers
            {}, // bufferMemoryBarriers
            {} // imageMemoryBarriers
        );
        cmd->buildAccelerationStructuresKHR({ tlasBuildGeometryInfo }, { &tlasRangeInfo });
```

同样，我们需要一个构建后 barrier，以确保构建过程中对加速结构的所有写入对后续读取或着色器访问可见：

```cpp
        // Post-build barrier
        vk::MemoryBarrier postBarrier {
            .srcAccessMask = vk::AccessFlagBits::eAccelerationStructureWriteKHR,
            .dstAccessMask = vk::AccessFlagBits::eAccelerationStructureReadKHR | vk::AccessFlagBits::eShaderRead
        };
        cmd->pipelineBarrier(
            vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, // srcStageMask
            vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR | vk::PipelineStageFlagBits::eFragmentShader, // dstStageMask
            {}, // dependencyFlags
            postBarrier, // memoryBarriers
            {}, // bufferMemoryBarriers
            {} // imageMemoryBarriers
        );
        endSingleTimeCommands(*cmd);
```

这些 barrier 对于正确的同步至关重要，可以防止竞态条件并确保加速结构对光线追踪着色器处于有效状态。

验证在 `drawFrame()` 中，在模型矩阵更新后调用了该函数：

```cpp
        updateUniformBuffer(frameIndex);
        // TASK06: Update the TLAS with the current model matrix
        updateTopLevelAS(ubo.model);
```

使用以下设置重新构建并运行：

```cpp
#define LAB_TASK_LEVEL 6
```

现在阴影应该正确更新了，因为加速结构和几何体动画已经同步：

![shadows dynamic](https://docs.vulkan.org/tutorial/latest/_images/images/38_TASK07_shadows_dynamic.gif)

---

> Ray Query vs Ray Tracing Pipeline：注意我们是如何在 fragment shader 中直接添加光线追踪效果（阴影）的。我们不需要单独的 ray generation shader 或任何新的管线。这就是 ray query（也称为内联光线追踪）的强大之处：我们将光线遍历集成到现有的渲染管线中。这使着色器逻辑保持统一，避免了额外的 GPU 着色器启动。在许多移动 GPU 上，这种方法不仅更方便，而且是必要的：如前所述，当前的移动设备大多支持 ray query 而不是完整的光线追踪管线，并且它们在 fragment shader 中高效地运行 ray query。这是我们在本实验中专注于 ray query 的关键原因。
