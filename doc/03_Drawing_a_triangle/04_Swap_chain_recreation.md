# Swap Chain 重建（Swap chain recreation）

> 原文：[https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/04_Swap_chain_recreation.html](https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/04_Swap_chain_recreation.html)

## 简介（Introduction）

我们现在拥有的应用程序成功地绘制了一个三角形，但有些情况它还没有正确处理。窗口表面可能会发生变化，以至于 swap chain 不再与其兼容。导致这种情况发生的原因之一是窗口大小的改变。我们必须捕获这些事件并重建 swap chain。

## 重建 Swap Chain

创建一个新的 `recreateSwapChain` 函数，该函数调用 `createSwapChain` 以及所有依赖 swap chain 或窗口大小的对象的创建函数。

```cpp
void recreateSwapChain() {
    device.waitIdle();

    createSwapChain();
    createImageViews();
}
```

我们首先调用 `vkDeviceWaitIdle`，因为就像上一章一样，我们不应该触碰可能仍在使用中的资源。显然，我们必须重建 swap chain 本身。Image view 也需要重建，因为它们直接基于 swap chain image。

为了确保在重建旧版本这些对象之前清理它们，我们应该将一些清理代码移到一个单独的函数中，以便从 `recreateSwapChain` 函数调用。我们将其称为 `cleanupSwapChain`：

```cpp
void cleanupSwapChain() {

}

void recreateSwapChain() {
    device.waitIdle();

    cleanupSwapChain();

    createSwapChain();
    createImageViews();
}
```

注意，为了简单起见，我们在这里不重建 render pass。理论上，swap chain image 格式在应用程序的生命周期内可能会改变，例如将窗口从标准范围显示器移到高动态范围显示器时。这可能需要应用程序重建 render pass 以确保动态范围之间的变化得到正确反映。

我们将把所有作为 swap chain 刷新一部分而重建的对象的清理代码从 `cleanup` 移到 `cleanupSwapChain`：

```cpp
void cleanupSwapChain() {
    swapChainImageViews.clear();
    swapChain = nullptr;
}

void cleanup() {
    cleanupSwapChain();

    glfwDestroyWindow(window);
    glfwTerminate();
}
```

注意，在 `chooseSwapExtent` 中，我们已经查询了新的窗口分辨率，以确保 swap chain image 具有正确的（新的）大小，因此无需修改 `chooseSwapExtent`（请记住，在创建 swap chain 时，我们已经必须使用 `glfwGetFramebufferSize` 来获取以像素为单位的表面分辨率）。

这就是重建 swap chain 所需的全部内容！然而，这种方法的缺点是，我们需要在创建新 swap chain 之前停止所有渲染。在旧 swap chain 上的 image 的绘制命令仍在进行中时，是可以创建新 swap chain 的。你需要将之前的 swap chain 传递给 `VkSwapchainCreateInfoKHR` 结构体中的 `oldSwapchain` 字段，并在使用完旧 swap chain 后立即将其销毁。

## 次优或过时的 Swap Chain（Suboptimal or out-of-date swap chain）

现在我们只需要弄清楚何时需要重建 swap chain，并调用我们新的 `recreateSwapChain` 函数。幸运的是，Vulkan 通常会在呈现期间告诉我们 swap chain 不再适用。`vk::raii::SwapchainKHR::acquireNextImage` 和 `vk::raii::Queue::presentKHR` 函数可以返回以下特殊值来表示这一情况。

- `vk::Result::eErrorOutOfDateKHR`：swap chain 已经与表面不兼容，无法再用于渲染。通常在窗口调整大小后发生。注意，`vk::Result::eErrorOutOfDateKHR` 实际上是一个错误代码，默认情况下会触发异常。通过定义 `VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS`，它被视为成功代码，因此可以由 `vk::raii::SwapchainKHR::acquireNextImage` 和 `vk::raii::Queue::presentKHR` 返回。

- `vk::Result::eSuboptimalKHR`：swap chain 仍然可以用于成功向表面呈现，但表面属性不再完全匹配。

```cpp
auto [result, imageIndex] = swapChain.acquireNextImage(UINT64_MAX, *presentCompleteSemaphores[frameIndex], nullptr);

if (result == vk::Result::eErrorOutOfDateKHR)
{
  recreateSwapChain();
  return;
}
if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
{
  assert(result == vk::Result::eTimeout || result == vk::Result::eNotReady);
  throw std::runtime_error("failed to acquire swap chain image!");
}
```

如果在尝试获取 image 时发现 swap chain 已经过时，则无法再向其呈现。因此，我们应该立即重建 swap chain，并在下一次 `drawFrame` 调用中重试。

你也可以在 swap chain 次优时这样做，但我选择在这种情况下继续，因为我们已经获取了一个 image。`vk::Result::eSuccess` 和 `vk::Result::eSuboptimalKHR` 都被认为是"成功"的返回码。

```cpp
result = queue.presentKHR(presentInfoKHR);
if ((result == vk::Result::eSuboptimalKHR) || (result == vk::Result::eErrorOutOfDateKHR))
{
  recreateSwapChain();
}
else
{
  // 除 eSuccess 外没有其他成功代码；对于任何错误代码，presentKHR 已经抛出了异常。
  assert(result == vk::Result::eSuccess);
}

frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
```

`vk::raii::Queue::presentKHR` 函数返回具有相同含义的相同值。在这种情况下，如果 swap chain 次优，我们也会重建它，因为我们想要最佳结果。

```cpp
const vk::PresentInfoKHR presentInfoKHR{.waitSemaphoreCount = 1,
                                        .pWaitSemaphores    = &*renderFinishedSemaphores[imageIndex],
                                        .swapchainCount     = 1,
                                        .pSwapchains        = &*swapChain,
                                        .pImageIndices      = &imageIndex};
result = queue.presentKHR(presentInfoKHR);
if ((result == vk::Result::eSuboptimalKHR) || (result == vk::Result::eErrorOutOfDateKHR) || framebufferResized)
{
  framebufferResized = false;
  recreateSwapChain();
}
else
{
  // 除 eSuccess 外没有其他成功代码；对于任何错误代码，presentKHR 已经抛出了异常。
  assert(result == vk::Result::eSuccess);
}
```

## 修复死锁（Fixing a deadlock）

如果我们现在尝试运行代码，可能会遇到死锁。调试代码后发现，应用程序到达 `vk::raii::Device::waitForFences` 后就再也不继续了。这是因为当 `vk::raii::SwapchainKHR::acquireNextImage` 返回 `vk::Result::eErrorOutOfDateKHR` 时，我们重建了 swap chain，然后从 `drawFrame` 返回。但在此之前，当前帧的 fence 已经被等待并重置。由于我们立即返回，没有任何工作被提交执行，fence 永远不会被发信号，导致 `vk::raii::Device::waitForFences` 永远挂起。

幸好有个简单的修复方法。延迟重置 fence，直到我们确实知道我们将用它提交工作为止。因此，如果我们提前返回，fence 仍然处于发信号状态，下次我们使用相同的 fence 对象时，`vk::raii::Device::waitForFences` 不会死锁。

`drawFrame` 的开头现在应该看起来像这样：

```cpp
auto fenceResult = device.waitForFences(*inFlightFences[frameIndex], vk::True, UINT64_MAX);
if (fenceResult != vk::Result::eSuccess)
{
  throw std::runtime_error("failed to wait for fence!");
}

auto [result, imageIndex] = swapChain.acquireNextImage(UINT64_MAX, *presentCompleteSemaphores[frameIndex], nullptr);

if (result == vk::Result::eErrorOutOfDateKHR)
{
    recreateSwapChain();
    return;
}
else if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
{
    assert(result == vk::Result::eTimeout || result == vk::Result::eNotReady);
    throw std::runtime_error("failed to acquire swap chain image!");
}

// 只有在确实提交工作时才重置 fence
device.resetFences(*inFlightFences[frameIndex]);
```

## 显式处理窗口大小调整（Handling resizes explicitly）

尽管许多驱动程序和平台在窗口调整大小后会自动触发 `vk::Result::eErrorOutOfDateKHR`，但这并不能保证一定发生。这就是为什么我们要添加一些额外的代码来显式处理调整大小的情况。首先，添加一个新的成员变量，标记发生了调整大小：

```cpp
std::vector<vk::raii::Fence> inFlightFences;

bool framebufferResized = false;
```

然后应修改 `drawFrame` 函数以同样检查此标志：

```cpp
if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || framebufferResized) {
    framebufferResized = false;
    recreateSwapChain();
}
else
{
    ...
}
```

在 `vk::raii::Queue::presentKHR` 之后执行此操作非常重要，以确保 semaphore 处于一致的状态，否则已发信号的 semaphore 可能永远不会被正确等待。现在，为了实际检测调整大小，我们可以使用 GLFW 框架中的 `glfwSetFramebufferSizeCallback` 函数来设置回调：

```cpp
void initWindow() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
}

static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {

}
```

我们将回调创建为 `static` 函数的原因是，GLFW 不知道如何正确调用带有正确 `this` 指针的成员函数来指向我们的 `HelloTriangleApplication` 实例。

但是，我们确实在回调中获得了 `GLFWwindow` 的引用，GLFW 还有另一个函数允许你在其中存储任意指针：`glfwSetWindowUserPointer`：

```cpp
window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
glfwSetWindowUserPointer(window, this);
glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
```

现在可以使用 `glfwGetWindowUserPointer` 在回调中检索此值，以正确设置标志：

```cpp
static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
    app->framebufferResized = true;
}
```

现在尝试运行程序并调整窗口大小，看看 framebuffer 是否确实随窗口正确调整大小。

## 处理最小化（Handling minimization）

swap chain 可能过时的另一种情况是一种特殊的窗口调整大小：窗口最小化。这种情况很特殊，因为它会导致 framebuffer 大小为 `0`。在本教程中，我们将通过扩展 `recreateSwapChain` 函数，在窗口重新回到前台之前暂停来处理这种情况：

```cpp
void recreateSwapChain() {
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }

    device.waitIdle();

    ...
}
```

对 `glfwGetFramebufferSize` 的初始调用处理了大小已经正确且 `glfwWaitEvents` 无需等待的情况。

恭喜，你现在已经完成了你的第一个行为良好的 Vulkan 程序！在下一章中，我们将摆脱 vertex shader 中的硬编码顶点，实际使用 vertex buffer。
