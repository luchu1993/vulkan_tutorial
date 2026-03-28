# 图像视图（Image Views）

> 原文：[https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/01_Presentation/02_Image_views.html](https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/01_Presentation/02_Image_views.html)

要在渲染管线中使用任何 `vk::Image`（包括 swap chain 中的图像），我们必须创建一个 `vk::raii::ImageView` 对象。image view 顾名思义就是对图像的一种"视图"。它描述了如何访问图像以及访问图像的哪个部分，例如，是否应将其视为没有任何 mipmap 级别的 2D 深度纹理。

在本章中，我们将编写一个 `createImageViews` 函数，为 swap chain 中的每张图像创建一个基本的 image view，以便我们在后续章节中可以将它们用作颜色目标（color targets）。

## 添加类成员（Class Member Addition）

首先添加一个类成员变量来存储 image view：

```cpp
std::vector<vk::raii::ImageView> swapChainImageViews;
```

## 创建函数并集成到初始化流程（Function Creation and Integration）

创建 `createImageViews` 函数，并在 swap chain 创建之后调用它：

```cpp
void initVulkan()
{
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
}

void createImageViews()
{
}
```

## 配置 ImageViewCreateInfo 结构体（ImageViewCreateInfo Structure Setup）

image view 的创建参数通过 `vk::ImageViewCreateInfo` 结构体来指定。`viewType`、`format` 和 `subresourceRange` 对每个 image view 来说是相同的。

```cpp
void createImageViews()
{
    assert(swapChainImageViews.empty());

    vk::ImageViewCreateInfo imageViewCreateInfo{ .viewType         = vk::ImageViewType::e2D,
                                                 .format           = swapChainSurfaceFormat.format,
                                                 .subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 } };
}
```

`viewType` 参数指定图像应被解读为 1D 纹理、2D 纹理、3D 纹理还是 cube map。

`format` 定义了颜色空间的分量配置。

## components 字段说明（Components Field Explanation）

`components` 字段允许你对颜色通道进行混排（swizzle）。例如，你可以将所有通道都映射到红色通道，以获得单色纹理。你也可以将常量值 `0` 和 `1` 映射到某个通道。对于我们的情况，我们将使用默认映射，用 `vk::ComponentSwizzle::eIdentity` 来明确表示：

```cpp
imageViewCreateInfo.components = {
    vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity};
```

## subresourceRange 字段详解（SubresourceRange Details）

`subresourceRange` 字段描述了图像的用途以及应该访问图像的哪个部分。我们的图像将被用作颜色目标，没有任何 mipmap 级别或多个层。

```cpp
imageViewCreateInfo.subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor, .levelCount = 1, .layerCount = 1};
```

如果你正在开发立体 3D 应用程序，那么你将创建一个具有多个层的 swap chain。然后你可以通过访问不同的层，为每张图像分别为左眼和右眼创建多个 image view。你应该期望显卡能够处理的多个 image view 的最大数量是 16。这种配置涵盖了大多数标准用例，VR/AR 头显通常只需要不超过 4 个同时存在的视图。

## 循环创建所有 image view（Loop Implementation）

```cpp
for (auto &image : swapChainImages)
{
    imageViewCreateInfo.image = image;
    swapChainImageViews.emplace_back( device, imageViewCreateInfo );
}
```

## 结语（Conclusion）

image view 足以让我们开始将图像用作纹理，但它还没有完全准备好用作渲染目标。这还需要一个额外的步骤，称为 framebuffer。但首先，我们必须设置图形管线（graphics pipeline）。
