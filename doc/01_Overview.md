# 概述（Overview）

> 原文：https://docs.vulkan.org/tutorial/latest/01_Overview.html

本章将首先介绍 Vulkan 及其所解决的问题。之后，我们将了解渲染第一个三角形所需的各项要素，从而建立一个宏观认识，以便将后续各章节与其对应起来。最后，我们将介绍 Vulkan API 的结构和通用使用模式。

---

## Vulkan 的起源（Origin of Vulkan）

与之前的图形 API 一样，Vulkan 被设计为一个跨平台的 GPU 抽象层。

大多数这类 API 的问题在于，它们设计之初所处的时代，图形硬件大多只有可配置的固定功能（configurable fixed functionality）。程序员必须以标准格式提供顶点数据，在光照和着色选项方面也完全受制于 GPU 制造商。

随着显卡架构日趋成熟，它们开始提供越来越多的可编程功能。所有这些新功能必须以某种方式整合进现有 API，这导致了不理想的抽象层，以及图形驱动程序在将程序员意图映射到现代图形架构时大量的猜测性工作。这正是为什么会有如此多的驱动更新来提升游戏性能，有时提升幅度甚至相当显著。由于这些驱动程序的复杂性，应用开发者还需要应对不同厂商之间的不一致，例如各厂商所接受的 shader 语法各有差异。

除这些新功能外，过去十年间也涌现了大量配备强大图形硬件的移动和嵌入式设备。这些移动 GPU 基于其能耗和空间要求，采用了不同的架构。其中一个例子是 tile-based rendering，若能给予程序员对该功能更多的控制权，将获得更好的性能提升。

这些旧有 API 的另一个局限来自其诞生年代：对多线程（multi-threading）的支持有限，这可能导致 CPU 侧的瓶颈。

Vulkan 通过从零开始、专为现代图形架构设计来解决这些问题。它通过允许程序员使用功能更丰富但更为详尽的 API 来明确表达其意图，从而降低了驱动程序开销，并允许多线程并行地创建和提交命令。它通过切换到使用单一编译器的标准化字节码格式来减少 shader 编译的不一致性。最后，它通过将图形与计算功能统一到单一 API 中，承认了现代显卡通用处理能力的现实。

---

## 编码规范（Coding conventions）

所有 Vulkan 函数、枚举和结构体均定义在 `vulkan.h` 头文件中，该头文件包含在由 LunarG 开发的 Vulkan SDK 中。我们将在下一章了解如何安装该 SDK。

本教程将使用 `vulkan.hpp` 头文件提供的 C++ Vulkan API，该头文件随官方 Vulkan SDK 一同提供。这个头文件在原始 C Vulkan API 之上提供了一个类型安全（type-safe）、RAII 友好（RAII-friendly）、且更具人体工学的接口，同时仍然与底层 Vulkan 函数和结构体保持非常紧密的底层映射关系。

---

## 画一个三角形需要什么（What it takes to draw a triangle）

现在我们来概览一下，在一个规范的 Vulkan 程序中渲染一个三角形所需的全部步骤。

此处介绍的所有概念将在后续章节中详细阐述，这里只是为了建立一个宏观认识，以便将各个独立组件联系起来。

### Step 1 — Instance 与 Physical Device 的选取

一个 Vulkan 应用从通过 `vk::Instance` 建立 Vulkan API 开始。Instance 的创建需要描述你的应用程序以及你将要使用的 API 扩展（extension）。创建 Instance 之后，你可以查询支持 Vulkan 的硬件，并选择一个或多个 `vk::PhysicalDevice` 用于操作。你可以查询诸如 VRAM 大小和设备能力等属性来选择合适的设备，例如优先选用独立显卡。

### Step 2 — Logical Device 与 Queue Family

选定合适的硬件设备后，你需要创建一个 `vk::Device`（逻辑设备），在其中更具体地描述你将要使用的 physical device 特性，例如多视口渲染（multi viewport rendering）和 64 位浮点数。

你还需要指定希望使用的 queue family。Vulkan 中的大多数操作，如绘制命令和内存操作，都是通过将其提交到 `vk::Queue` 来异步执行的。Queue 从 queue family 中分配，每个 queue family 在其 queue 中支持一组特定的操作。

例如，可以有分别用于图形（graphics）、计算（compute）和内存传输（memory transfer）操作的独立 queue family。Queue family 的可用性也可作为选择 physical device 的区分因素之一。

一个支持 Vulkan 的设备完全可能不提供任何图形功能；然而，当今所有支持 Vulkan 的显卡通常都支持我们所需的全部 queue 操作。

### Step 3 — Window Surface 与 Swap Chain

除非你只对离屏渲染（offscreen rendering）感兴趣，否则你需要创建一个窗口来呈现渲染好的图像。窗口可以使用原生平台 API 或 GLFW、SDL 等库来创建。本教程将使用 GLFW，更多细节将在下一章介绍。

要实际渲染到窗口，我们还需要两个部件：window surface（`vk::SurfaceKHR`）和 swap chain（`vk::SwapchainKHR`）。注意 `KHR` 后缀，这意味着这些对象是 Vulkan 扩展的一部分。Vulkan API 本身完全与平台无关，因此我们需要使用标准化的 WSI（Window System Interface）扩展来与窗口管理器交互。

Surface 是对渲染目标窗口的跨平台抽象，通常通过提供原生窗口句柄的引用来实例化，例如 Windows 上的 `HWND`。幸运的是，GLFW 库内置了处理这些平台特定细节的函数。

Swap chain 是一组渲染目标的集合。其基本目的是确保我们当前正在渲染的图像与屏幕上正在显示的图像不同。这对于确保屏幕上只显示完整的图像非常重要。每次我们想要绘制一帧时，都必须向 swap chain 请求一张可供渲染的图像。当我们完成一帧的绘制后，图像被归还给 swap chain，以便在某个时刻呈现到屏幕上。

渲染目标的数量以及将完成图像呈现到屏幕上的条件取决于 present mode。常见的 present mode 有双缓冲（double buffering，即 vsync）和三缓冲（triple buffering）。我们将在 swap chain 创建章节中深入了解这些内容。

某些平台允许你通过 `VK_KHR_display` 和 `VK_KHR_display_swapchain` 扩展，直接渲染到显示器而无需与任何窗口管理器交互。这些扩展允许你创建一个代表整个屏幕的 surface，例如可用于实现你自己的窗口管理器。

### Step 4 — Image View 与 Dynamic Rendering

要绘制到从 swap chain 获取的图像上，我们通常需要将其封装为 `vk::ImageView` 和 `vk::Framebuffer`。Image view 引用图像中用于特定用途的特定部分，framebuffer 则引用用作颜色（color）、深度（depth）和模板（stencil）目标的 image view。

由于 swap chain 中可能有多张不同的图像，我们需要预先为每张图像创建对应的 image view 和 framebuffer，并在绘制时选择正确的那个。

### Step 5 — Dynamic Rendering 概述

在早期版本的 Vulkan 中，render pass 定义了渲染操作与 framebuffer 的交互方式，指定所用图像的类型（例如颜色、深度）以及如何处理其内容（例如清除、加载或存储）。`vk::RenderPass` 定义子 pass（subpass）和 attachment 的使用方式，`vk::Framebuffer` 则将具体的 image view 绑定到这些 attachment 上。

然而，随着 Vulkan 1.3 引入的 dynamic rendering，你不再需要创建 `vk::Framebuffer`。Dynamic rendering 消除了对预定义 render pass 和 framebuffer 的需求，允许在命令录制（command recording）时直接指定渲染 attachment。这使 API 大为简化，因为我们可以动态定义渲染目标，而无需管理 framebuffer 的开销。

使用 dynamic rendering 后，你不再需要预定义 `vk::RenderPass` 或 `vk::Framebuffer`。转而在命令录制开始时指定渲染 attachment，使用 `vk::beginRendering` 以及 `vk::RenderingInfo` 等结构体来动态提供所有必要的 attachment 信息。

在我们最初的三角形渲染应用中，我们将使用 dynamic rendering 将单张图像指定为颜色目标（color target），并在绘制前将其清除为纯色。

### Step 6 — Graphics Pipeline

Vulkan 中的 graphics pipeline 通过创建 `VkPipeline` 对象来建立。它描述了图形卡的可配置状态，例如视口大小（viewport size）和深度缓冲操作（depth buffer operation），以及通过 `vk::ShaderModule` 对象指定的可编程状态。`vk::ShaderModule` 对象由 shader 字节码创建。驱动程序还需要知道 pipeline 中将使用哪些渲染目标，这通过引用 render pass 来指定。

Vulkan 与现有 API 相比最显著的特点之一是，几乎所有 graphics pipeline 的配置都必须提前设置好。这意味着，如果你想切换到不同的 shader，或者稍微改变顶点布局，就需要完整地重建整个 graphics pipeline。这意味着你必须提前为渲染操作所需的所有不同组合创建好多个 `vk::Pipeline` 对象。只有少数基本配置，如视口大小（viewport size）和清除颜色（clear color），可以动态修改。所有状态也需要显式描述；例如，没有默认的颜色混合状态（color blend state）。

好消息是，由于你做的工作等价于提前编译（ahead-of-time compilation）而非即时编译（just-in-time compilation），驱动程序拥有了更多的优化机会。运行时性能更加可预测，因为诸如切换到不同 graphics pipeline 这类大型状态变更都变得非常显式。

### Step 7 — Command Pool 与 Command Buffer

如前所述，Vulkan 中许多我们想要执行的操作，例如绘制操作，都需要提交到 queue。这些操作在提交前必须先录制到 `vk::CommandBuffer` 中。这些 command buffer 从与特定 queue family 关联的 `vk::CommandPool` 中分配。

传统上，要绘制一个简单的三角形，我们需要录制一个包含以下操作的 command buffer：

- 开始 render pass（Begin the render pass）
- 绑定 graphics pipeline（Bind the graphics pipeline）
- 绘制三个顶点（Draw three vertices）
- 结束 render pass（End the render pass）

然而，使用 dynamic rendering 后，情况有所变化。不再是"开始"和"结束" render pass，而是在调用 `vk::BeginRendering` 时直接定义渲染 attachment。这通过允许你动态指定必要的 attachment，简化了流程，使其更能适应 swap chain 图像被动态选取的场景。

因此，你无需为 swap chain 中的每张图像录制 command buffer，也无需每帧重复录制同一个 command buffer。整个操作变得更加精简高效，使 Vulkan 在处理渲染场景时更加灵活。

### Step 8 — 主循环（Main Loop）

现在绘制命令已经封装到 command buffer 中，主循环就相当简单了。我们首先通过 `device.acquireNextImageKHR` 从 swap chain 获取一张图像。然后为该图像选取合适的 command buffer，并通过 `graphicsQueue.submit()` 执行它。最后，通过 `presentQueue.presentKHR(presentInfo)` 将图像归还给 swap chain，以便呈现到屏幕上。

提交到 queue 的操作是异步执行的。因此，我们必须使用诸如 semaphore 这样的同步对象来确保正确的执行顺序。绘制命令 buffer 的执行必须设置为等待图像获取（image acquisition）完成；否则可能出现我们开始向一张仍在被读取用于屏幕显示的图像进行渲染的情况。反过来，`presentQueue.presentKHR(presentInfoKHR)` 调用也需要等待渲染完成，为此我们将使用第二个 semaphore，它在渲染完成后被触发（signal）。

---

## 小结（Summary）

这场快速浏览应该能让你对画出第一个三角形所需完成的工作有一个基本的认识。

一个真实的程序包含更多步骤，例如分配 vertex buffer、创建 uniform buffer 以及上传纹理图像（texture image），这些将在后续章节中介绍。不过，我们会从简单的开始，因为 Vulkan 本身已经有相当陡峭的学习曲线。

需要注意的是，我们会稍微走一点捷径，最初将顶点坐标直接嵌入 vertex shader，而不是使用 vertex buffer。这是因为管理 vertex buffer 首先需要对 command buffer 有所了解。

简而言之，要绘制第一个三角形，我们需要：

- 创建一个 Instance
- 选择一块支持的显卡（PhysicalDevice）
- 创建用于绘制和呈现的 Device 和 Queue
- 创建窗口、window surface 和 swap chain
- 将 swap chain 图像封装为 `VkImageView`
- 配置 dynamic rendering
- 配置 graphics pipeline
- 分配并录制包含绘制命令的 command buffer，覆盖所有可能的 swap chain 图像
- 通过获取图像、提交正确的绘制 command buffer 并将图像归还给 swap chain 来绘制帧

步骤很多，但每个步骤的目的都将在后续章节中清晰明了地说明。如果你对某个单独步骤与整个程序的关系感到困惑，应当回到本章重新参考。

---

## API 概念（API concepts）

本章最后将简要介绍 Vulkan API 在底层的结构方式。

例如，对象的创建通常遵循以下模式：

```cpp
vk::XXXCreateInfo createInfo{};
createInfo.sType = vk::StructureType::eXXXCreateInfo;
createInfo.pNext = nullptr;
createInfo.foo = ...;
createInfo.bar = ...;

vk::XXX object;
try {
    object = device.createXXX(createInfo);
} catch (vk::SystemError& err) {
    std::cerr << "Failed to create object: " << err.what() << std::endl;
    return false;
}
```

Vulkan 中的许多结构体都要求你在 `sType` 成员中显式指定结构体的类型。`pNext` 成员可以指向一个扩展结构体，在本教程中始终为 `nullptr`。

创建或销毁对象的函数会有一个 `VkAllocationCallbacks` 参数，允许你为驱动程序内存使用自定义分配器，在本教程中同样留为 `nullptr`。

几乎所有函数都返回一个 `vk::Result`，其值为 `vk::result::eSuccess` 或一个错误码。规范描述了每个函数可能返回的错误码及其含义。此类调用的失败通过 C++ 异常上报。异常会提供更多关于错误的信息，包括前述的 `vk::Result`，这使我们能够从一次调用中检查多条命令，同时保持命令语法的简洁。

---

## Validation Layer

如前所述，Vulkan 以高性能和低驱动程序开销为设计目标。因此，它默认只包含非常有限的错误检查和调试能力。如果你做了错误的操作，驱动程序往往会直接崩溃而不是返回错误码，更糟的情况是，它在你的显卡上看起来正常运行，却在其他显卡上完全失败。

Vulkan 允许你通过一个叫做 validation layer 的特性来启用广泛的检查。Validation layer 是可以插入在 API 与图形驱动程序之间的代码片段，用于执行诸如对函数参数进行额外检查、追踪内存管理问题等操作。其一个重要优点是，你可以在开发期间启用它们，然后在发布应用时将其完全禁用，以实现零额外开销。

任何人都可以编写自己的 validation layer，但 LunarG 的 Vulkan SDK 提供了一套标准的 validation layer，这也是本教程将使用的。你还需要注册一个回调函数来接收来自这些 layer 的调试信息。

由于 Vulkan 对每一个操作都如此明确，validation layer 又如此详尽，实际上与 OpenGL 和 Direct3D 相比，找出屏幕为什么是黑的可能反而容易得多！
