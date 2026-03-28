# Acceleration Structures（Acceleration Structures）

> 原文：https://docs.vulkan.org/tutorial/latest/courses/18_Ray_tracing/02_Acceleration_structures.html

## 目标（Objective）

为模型的几何体创建 Bottom-Level Acceleration Structures（BLAS），并创建一个引用这些 BLAS 的 Top-Level Acceleration Structure（TLAS）。将 TLAS 绑定到着色器，以便我们可以使用 Ray Query。

在场景中投射光线时，我们需要一个能够快速识别光线命中哪个三角形的优化结构。GPU 使用将几何体分组为包围盒的加速结构，允许跳过场景的大部分内容。光线遍历沿着树向下进行，高效地缩小到被击中的三角形。其具体实现依赖于 GPU，对用户是不透明的。

![bounding boxes](https://docs.vulkan.org/tutorial/latest/_images/images/38_bounding_boxes.png)

我们的场景是一个简单的 3D 模型（桌上的植物），从 OBJ 文件加载。提供的代码已经将模型的顶点、索引、法线和纹理加载到 buffer 中。它将模型分为多个子网格，每个子网格有自己的材质。

- BLAS 保存一个网格或对象的几何体（三角形）
- TLAS 保存 BLAS 的实例（带变换），以形成完整场景
- 我们将为每个不同的网格/材质创建一个 BLAS，并创建一个引用它们的 TLAS
- Ray query 将使用 TLAS

在 Vulkan 中，构建加速结构涉及几个步骤：描述几何体、查询构建大小、分配 buffer、创建 AS 句柄，然后发出构建命令。

---

## Task 2：为每个子网格创建 BLAS（Create a BLAS for each submesh）

进入 `createAccelerationStructures` 的定义。在这里，我们将首先为模型中的每个子网格创建 BLAS。代码中已有一个迭代子网格的循环，其中包含几何体数据。

首先，我们需要描述 BLAS 的几何体。`vk::AccelerationStructureGeometryKHR` 结构体用于此目的：

```cpp
// Prepare the geometry data
auto trianglesData = vk::AccelerationStructureGeometryTrianglesDataKHR{
    .vertexFormat = vk::Format::eR32G32B32Sfloat,
    .vertexData = vertexAddr,
    .vertexStride = sizeof(Vertex),
    .maxVertex = submesh.maxVertex,
    .indexType = vk::IndexType::eUint32,
    .indexData = indexAddr + submesh.indexOffset * sizeof(uint32_t)
};
vk::AccelerationStructureGeometryDataKHR geometryData(trianglesData);
vk::AccelerationStructureGeometryKHR blasGeometry{
    .geometryType = vk::GeometryTypeKHR::eTriangles,
    .geometry = geometryData,
    .flags = vk::GeometryFlagBitsKHR::eOpaque
};
```

然后将其记录在构建信息结构体中：

```cpp
vk::AccelerationStructureBuildGeometryInfoKHR blasBuildGeometryInfo{
    .type = vk::AccelerationStructureTypeKHR::eBottomLevel,
    .mode = vk::BuildAccelerationStructureModeKHR::eBuild,
    .geometryCount = 1,
    .pGeometries = &blasGeometry,
};
```

接下来，我们需要查询 BLAS 的内存需求：

```cpp
// TASK02: Query the memory sizes that will be needed for this BLAS
vk::AccelerationStructureBuildSizesInfoKHR blasBuildSizes =
    device.getAccelerationStructureBuildSizesKHR(
        vk::AccelerationStructureBuildTypeKHR::eDevice,
        blasBuildGeometryInfo,
        { primitiveCount }
);
```

这个辅助函数使用 `vkGetAccelerationStructureBuildSizesKHR()` 并返回 BLAS 所需的内存大小。我们需要分配：

- 一个用于 BLAS 本身的 buffer
- 另一个用于构建过程中使用的暂存空间的 buffer

然后，我们可以创建这些 buffer 并将它们存储在持久数组中，因为稍后会用到它们。

我们还需要创建 BLAS 句柄本身，这通过 `vk::AccelerationStructureCreateInfoKHR` 和使用 `vkCreateAccelerationStructureKHR()` 的辅助函数来完成。句柄存储在一个向量中以供后续使用（记住我们每个子网格需要一个）：

```cpp
// TASK02: Create and store the BLAS handle
vk::AccelerationStructureCreateInfoKHR blasCreateInfo{
    .buffer = blasBuffers[i],
    .offset = 0,
    .size = blasBuildSizes.accelerationStructureSize,
    .type = vk::AccelerationStructureTypeKHR::eBottomLevel,
};
blasHandles.emplace_back(device.createAccelerationStructureKHR(blasCreateInfo));
// Save the BLAS handle in the build info structure
blasBuildGeometryInfo.dstAccelerationStructure = blasHandles[i];
```

下图总结了我们到目前为止创建的所有结构体和 buffer：

![BLAS structures](https://docs.vulkan.org/tutorial/latest/_images/images/38_TASK02_blas_structures.png)

为了将所有内容整合在一起，我们需要提交一个命令缓冲区以在 GPU 上构建 BLAS。这通过 `vkCmdBuildAccelerationStructuresKHR()` 完成，它接受构建信息和一个 range。该 range 提供了一次构建多个几何体的灵活性，但这里每个 BLAS 只有一个几何体，因此保持简单：

```cpp
// TASK02: Prepare the build range for the BLAS
vk::AccelerationStructureBuildRangeInfoKHR blasRangeInfo{
    .primitiveCount = primitiveCount,
    .primitiveOffset = 0,
    .firstVertex = firstVertex,
    .transformOffset = 0
};
```

最后，准备并提交一个命令缓冲区，它保存了 bottom level acceleration structure 的有效句柄：

```cpp
// TASK02: Build the BLAS
auto cmd = beginSingleTimeCommands();
cmd->buildAccelerationStructuresKHR({ blasBuildGeometryInfo }, { &blasRangeInfo });
endSingleTimeCommands(*cmd);
```

![BLAS build](https://docs.vulkan.org/tutorial/latest/_images/images/38_TASK02_blas_build.png)

现在你已经为场景中的每个模型创建了一个 BLAS。接下来，我们需要将它们全部组合到一个 TLAS 中，该 TLAS 将被我们的 fragment shader 使用。

---

## Task 3：使用 BLAS 实例创建 TLAS（Create a TLAS with instances of the BLASes）

现在我们有了 BLAS，我们需要创建一个引用它们的 TLAS。TLAS 将持有 BLAS 的实例，允许我们通过变换（位置、旋转、缩放）将它们放置在场景中。

我们可以在创建 BLAS 的同一个子网格循环中创建实例。对于每个子网格，我们将创建一个引用相应 BLAS 句柄的实例。`vk::AccelerationStructureInstanceKHR` 结构体用于此目的：

```cpp
// TASK03: Create a BLAS instance for the TLAS
vk::AccelerationStructureDeviceAddressInfoKHR addrInfo{
    .accelerationStructure = *blasHandles[i]
};
vk::DeviceAddress blasDeviceAddr = device.getAccelerationStructureAddressKHR(addrInfo);
vk::AccelerationStructureInstanceKHR instance{
    .transform = tm,
    .mask = 0xFF,
    .accelerationStructureReference = blasDeviceAddr
};
instances.push_back(instance);
```

注意我们需要使用 `vkGetAccelerationStructureDeviceAddressKHR()` 获取 BLAS 的设备地址。我们暂时将变换矩阵设为单位矩阵，稍后在实验中会重新讨论。

现在所有实例都存储在一个向量中，我们需要为 TLAS 准备实例数据。这涉及创建一个保存实例数据的 buffer。

使用与 BLAS 非常相似的方法，我们需要为 TLAS 构建准备数据、查询 buffer 大小、分配 buffer、创建 TLAS 句柄，并发出构建命令。下图突出显示了 TLAS 所需的主要变化：

![TLAS structures](https://docs.vulkan.org/tutorial/latest/_images/images/38_TASK03_tlas_structures.png)

为了准备 TLAS 的几何体数据，我们将使用 `vk::GeometryTypeKHR::eInstances` 来表明我们正在从 BLAS 的实例构建 TLAS：

```cpp
// TASK03: Prepare the geometry (instance) data
auto instancesData = vk::AccelerationStructureGeometryInstancesDataKHR{
    .arrayOfPointers = vk::False,
    .data = instanceAddr
};
vk::AccelerationStructureGeometryDataKHR geometryData(instancesData);
vk::AccelerationStructureGeometryKHR tlasGeometry{
    .geometryType = vk::GeometryTypeKHR::eInstances,
    .geometry = geometryData
};
```

然后将其记录在构建信息结构体中：

```cpp
vk::AccelerationStructureBuildGeometryInfoKHR tlasBuildGeometryInfo{
    .type = vk::AccelerationStructureTypeKHR::eTopLevel,
    .mode = vk::BuildAccelerationStructureModeKHR::eBuild,
    .geometryCount = 1,
    .pGeometries = &tlasGeometry
};
```

接下来，我们需要查询 TLAS 的内存需求：

```cpp
// TASK03: Query the memory sizes that will be needed for this TLAS
vk::AccelerationStructureBuildSizesInfoKHR tlasBuildSizes =
    device.getAccelerationStructureBuildSizesKHR(
        vk::AccelerationStructureBuildTypeKHR::eDevice,
        tlasBuildGeometryInfo,
        { primitiveCount }
);
```

再次创建必要的 buffer。要创建 TLAS 句柄，我们像之前一样使用 `vkCreateAccelerationStructureKHR()`：

```cpp
// TASK03: Create and store the TLAS handle
vk::AccelerationStructureCreateInfoKHR tlasCreateInfo{
    .buffer = tlasBuffer,
    .offset = 0,
    .size = tlasBuildSizes.accelerationStructureSize,
    .type = vk::AccelerationStructureTypeKHR::eTopLevel,
};
tlas = device.createAccelerationStructureKHR(tlasCreateInfo);
// Save the TLAS handle in the build info structure
tlasBuildGeometryInfo.dstAccelerationStructure = tlas;
```

再一次，我们需要为 TLAS 准备构建 range。这与 BLAS 类似，但现在我们使用实例数量。然后我们可以提交命令缓冲区来构建 TLAS：

```cpp
 // TASK03: Prepare the build range for the TLAS
 vk::AccelerationStructureBuildRangeInfoKHR tlasRangeInfo{
     .primitiveCount = primitiveCount,
     .primitiveOffset = 0,
     .firstVertex = 0,
     .transformOffset = 0
 };
// TASK03: Build the TLAS
auto cmd = beginSingleTimeCommands();
cmd->buildAccelerationStructuresKHR({ tlasBuildGeometryInfo }, { &tlasRangeInfo });
endSingleTimeCommands(*cmd);
```

完成！你现在已经创建了一个引用模型中所有子网格 BLAS 的 TLAS。TLAS 已准备好在 fragment shader 的 ray query 中使用。

---

## Task 4：将加速结构绑定到着色器（Bind the acceleration structure to the shader）

为了在着色器中访问加速结构，我们需要为 TLAS 添加一个 descriptor set binding。这在 `createDescriptorSetLayout()` 函数中完成（你现在可以忽略更高编号的 binding）：

```cpp
// TASK04: The acceleration structure uses binding 1
std::array global_bindings = {
    vk::DescriptorSetLayoutBinding( 0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, nullptr),
    vk::DescriptorSetLayoutBinding( 1, vk::DescriptorType::eAccelerationStructureKHR, 1, vk::ShaderStageFlagBits::eFragment, nullptr),
    vk::DescriptorSetLayoutBinding( 2, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eFragment, nullptr),
    vk::DescriptorSetLayoutBinding( 3, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eFragment, nullptr),
    vk::DescriptorSetLayoutBinding( 4, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eFragment, nullptr)
};
```

接下来，我们需要更新 descriptor set 以绑定 TLAS。这在 `updateDescriptorSets()` 函数中完成：

```cpp
vk::WriteDescriptorSetAccelerationStructureKHR asInfo{
    .accelerationStructureCount = 1,
    .pAccelerationStructures = {&*tlas}
};
vk::WriteDescriptorSet asWrite{
    .pNext = &asInfo,
    .dstSet = globalDescriptorSets[i],
    .dstBinding = 1,
    .dstArrayElement = 0,
    .descriptorCount = 1,
    .descriptorType = vk::DescriptorType::eAccelerationStructureKHR
};
```

然后调用 `vkUpdateDescriptorSets()`，将 TLAS 包含在列表中：

```cpp
std::array<vk::WriteDescriptorSet, 4> descriptorWrites{bufferWrite, asWrite, indexBufferWrite, uvBufferWrite};
device.updateDescriptorSets(descriptorWrites, {});
```

最后，在着色器中添加对应的属性：

```slang
// TASK04: Acceleration structure binding
[[vk::binding(1,0)]]
RaytracingAccelerationStructure accelerationStructure;
```

使用以下设置重新构建并运行：

```cpp
#define LAB_TASK_LEVEL 4
```

你不会看到视觉上的差异，但请放心，你的 Acceleration Structures 现在已经设置好并准备好在 fragment shader 中使用了。
