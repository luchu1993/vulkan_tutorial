# 开发环境（Development Environment）

> 原文：https://docs.vulkan.org/tutorial/latest/02_Development_environment.html

在本章中，我们将配置开发 Vulkan 应用程序的环境，并安装一些有用的库。除编译器外，我们将使用的所有工具均兼容 Windows、Linux 和 macOS，但安装步骤略有差异，因此在此分别说明。

---

## 获取代码

首先，我们需要从 GitHub 仓库克隆本教程的代码，这需要安装 git 版本控制系统。

安装好 git 后，可以通过以下命令在本地克隆仓库：

```
git clone https://github.com/KhronosGroup/Vulkan-Tutorial
```

这将把仓库克隆到当前目录下一个名为 `Vulkan-Tutorial` 的新文件夹中。各章节的源文件位于 `attachments` 文件夹中。

---

## 依赖安装脚本

为了简化安装流程，我们为 Windows 和 Linux 提供了依赖安装脚本。

### Windows

对于 Windows，我们提供了一个使用 vcpkg 安装所有必需依赖的脚本：

- 确保已安装 vcpkg。如未安装，请参照 https://github.com/microsoft/vcpkg 上的说明进行安装。
- 运行 `scripts/install_dependencies_windows.bat` 脚本。
- 按照提示安装 Vulkan SDK。

虽然我们使用 vcpkg 来支持此安装脚本，但完整的安装过程在下文有详细说明，无需使用安装脚本或 vcpkg 也可以完成。这只是为了让安装流程更便捷。

### Linux

对于 Linux，我们提供了一个可检测你的包管理器并安装所有必需依赖的脚本：

- 运行 `scripts/install_dependencies_linux.sh` 脚本。
- 按照提示安装 Vulkan SDK。

如果你希望手动安装依赖，或者你使用的是 macOS，请参照下方平台专属说明。

---

## 通用注意事项

### Vulkan SDK

开发 Vulkan 应用程序最重要的部分是 SDK。它包含了头文件、标准 validation layer、调试工具以及 Vulkan 函数的 loader。loader 在运行时从驱动程序中查找函数，类似于 OpenGL 的 GLEW（如果你熟悉的话）。

SDK 可以从 LunarG 网站下载。完成安装后，请留意 SDK 的安装位置。

我们首先要做的是验证你的显卡和驱动程序是否正确支持 Vulkan。进入 SDK 的安装目录，打开 `bin` 目录，运行 `vkcube` 演示程序。

该目录中还有另一个对开发很有用的程序。命令行程序 `slangc` 将用于把人类可读的 Slang Shading Language 编译为字节码。我们将在 shader modules 章节中深入介绍这一点。`bin` 目录中还包含 Vulkan loader 和 validation layer 的二进制文件，`lib` 目录包含库文件。最后，`include` 目录包含 Vulkan 头文件。你可以自由探索其他文件，但本教程中不会用到它们。

为了自动设置 VulkanSDK 将使用的环境变量（用于简化 CMake 项目配置和其他工具），我们建议在 Linux 上使用 `setup-env` 脚本。你可以将此脚本添加到终端的自动启动配置或 IDE 设置中，以确保这些环境变量在所有会话中均可用。

如果收到错误消息，请确保你的驱动程序是最新版本，包含 Vulkan runtime，并且你的显卡受到支持。有关各主要厂商驱动程序的链接，请参阅简介章节。

### CMake

尽管跨平台项目有各种麻烦，CMake 已成为业界通用的标准工具。它允许开发者创建一个全项目范围的构建描述文件，负责设置和配置创建任意项目所需的所有支持工具。

其他具有类似功能的构建系统也存在，例如 bazel，但没有一个像 CMake 那样被广泛使用和接受。

关于如何使用 CMake 的完整说明超出了本教程的范围，但更多细节可以在 CMake 官方文档中找到。

Vulkan SDK 支持使用 `find_package`。要在你的项目中使用它，可以将 `*-config.cmake` 的搜索路径添加到 `find_package` config 调用的 `HINTS` 部分，例如：

```cmake
find_package(slang CONFIG HINTS "$ENV{VULKAN_SDK}/lib/cmake")
```

在未来，`FindVulkan.cmake` 可能会迁移到 `*-config.cmake` 标准，但在撰写本文时，建议从 VulkanSamples 获取 `FindVulkan.cmake`，因为 Kitware 提供的版本已被弃用且在 macOS 构建上存在 bug。你可以在代码目录的 `FindVulkan.cmake` 中找到它。

使用 `FindVulkan.cmake` 是项目特定的文件，你可以将其取出并根据需要修改，以便在你的构建环境中正常工作，并进一步根据需求定制。Khronos 在 VulkanSamples 中分发的版本经过充分测试，是一个很好的起点。

要使用它，将其添加到你的 `CMAKE_MODULE_PATH`，如下所示：

```cmake
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/CMake")
```

这将允许其他通过 `Find*.cmake` 分发的项目放置在同一文件夹中。参见附带的 `CMakeLists.txt` 了解一个可运行项目的示例。

Vulkan 支持 C++20 中引入的 C++ 模块。C++ 模块的一大优势是在享有 C++ 全部优势的同时，避免了漫长的编译时间。为此，`.cppm` 文件必须针对目标设备进行编译。本教程演示了如何利用 C++ 模块。然而，为了最大化编译器和 IDE 的兼容性，attachments 模板默认禁用了 C++20 模块支持，但如果你的工具链支持，建议启用它。

要在 attachments 模板中启用 Vulkan C++20 模块，请使用以下命令配置 CMake：

```
cmake -DENABLE_CPP20_MODULE=ON ..
```

启用后，`CMakeLists.txt` 包含自动构建模块所需的全部指令。相关代码片段如下：

```cmake
find_package (Vulkan REQUIRED)
# set up Vulkan C++ module (enabled when ENABLE_CPP20_MODULE=ON)
add_library(VulkanCppModule)
add_library(Vulkan::cppm ALIAS VulkanCppModule)
target_compile_definitions(VulkanCppModule PUBLIC
        VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
        VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1
)
target_include_directories(VulkanCppModule PRIVATE "${Vulkan_INCLUDE_DIR}")
target_link_libraries(VulkanCppModule PUBLIC Vulkan::Vulkan)
set_target_properties(VulkanCppModule PROPERTIES CXX_STANDARD 20)
target_sources(VulkanCppModule
        PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS "${Vulkan_INCLUDE_DIR}"
        FILES "${Vulkan_INCLUDE_DIR}/vulkan/vulkan.cppm"
)
```

`VulkanCppModule` target 只需定义一次，然后将其添加到你的消费项目的依赖中，它将自动被构建。你无需再将 `Vulkan::Vulkan` 也添加到你的项目中。

```cmake
target_link_libraries (${PROJECT_NAME} Vulkan::cppm)
```

如果你选择保持模块禁用（默认状态），可以继续使用传统的基于头文件的 include（例如 `#include <vulkan/vulkan_raii.hpp>`）。attachments 中的示例代码已编写为两种方式均可编译，仅在 `ENABLE_CPP20_MODULE=ON`（即定义了 `USE_CPP20_MODULES`）时才会 import 模块。

### 窗口管理（Window Management）

如前所述，Vulkan 本身是平台无关的 API，不包含创建窗口来显示渲染结果的工具。为了充分利用 Vulkan 跨平台的优势，我们将使用 GLFW 库来创建窗口，它支持 Windows、Linux 和 macOS。此外还有其他可用于此目的的库，例如 SDL，但 GLFW 的优势在于它除窗口创建外，还抽象了 Vulkan 中其他一些平台特定的细节。

一个不幸的缺点是 GLFW 不能在 Android 或 iOS 上运行；它是仅限桌面的解决方案。SDL 提供了对移动端的支持；然而，移动端的窗口支持最好通过与操作系统直接接口来实现，例如在 Android 上使用 JNI。

### GLM

与 DirectX 12 不同，Vulkan 不包含线性代数运算库，因此我们需要自行下载。GLM 是一个专为图形 API 使用而设计的优秀库，也常与 OpenGL 一起使用。

### 纹理库（Texturing library）

Vulkan 本身不支持读取 png、jpeg 或 ktx 等各种纹理资源。然而，这是一个很大的话题，完整深入所有格式超出了本教程的范围。在本教程中，我们将使用 stb 作为加载纹理的依赖库。我们确实建议研究 ktx，以充分利用专为图形应用设计的纹理格式。

### 模型库（Modeling library）

模型格式种类繁多，到处都暴露了大量细节。总体而言，在 Vulkan 和其他图形 API 中，最重要的信息是顶点信息、纹理坐标，以及潜在的漫反射颜色（diffuse color）细节。GLTF 是一种功能丰富的高级模型格式，在跨平台库中具有易于支持的特性。然而，在本教程中，我们将使用 tinyobjloader，因为其极度简洁。我们建议仅在小型且不复杂的项目中使用 tinyobjloader 库。

---

## Windows

在 Windows 上开发最方便的是使用 Visual Studio。CLion 和 Android Studio 在 Windows 上也运行良好，然而 Visual Studio 非常流行且受到很好的支持，因此我们将讨论在其中管理依赖的方法。要获得完整的 C++20 支持，你需要使用 2019 版本以后的任何版本。下面的步骤是为 VS 2022 编写的。

### 包管理（Package management）

对于所有平台，我们建议使用平台管理工具。Windows 本身并不依赖包管理，所以这对 Windows 用户来说是一个陌生的概念。然而，Microsoft 已经推出了一个出色的包管理工具，可以跨平台使用。VCPkg 还包含了设置所有必需 CMake 配置的功能。我们建议参阅其优秀的文档，了解如何在 Windows 项目中使用 CMake 的详细信息。

这种配置允许 Windows 开发者在 Visual Studio 中使用 CMake 进行原生开发，集成效果相当不错。

另外，CLion 在所有平台上原生支持 `CMakeLists.txt` 项目，其工作方式与 Android Studio 完全相同，且是免费的 IDE。

### GLFW

我们建议如前所述使用 vcpkg 安装包，为此，在命令行运行：`vcpkg install glfw3`

如果你不想使用 vcpkg，可以在官方网站上找到最新版本的 GLFW。

在本教程中，我们将使用 64 位的二进制文件，当然你也可以选择以 32 位模式构建。在这种情况下，确保链接 `Lib32` 目录中的 Vulkan SDK 二进制文件而不是 `Lib`。下载后，将压缩包解压到一个方便的位置。我们选择在文档中的 Visual Studio 目录下创建一个 `Libraries` 目录。

![GLFW directory](https://docs.vulkan.org/tutorial/latest/_images/images/glfw_directory.png)

### GLM

作为一个纯图形 API，Vulkan 不包含线性代数运算库，因此我们需要自行下载。

GLM 同样可以通过 vcpkg 安装：`vcpkg install glm`

另外，GLM 是一个仅有头文件的库，可以直接下载 GLM，它专为与图形 API 配合使用而设计，也常与 OpenGL 一起使用。

![Library directory](https://docs.vulkan.org/tutorial/latest/_images/images/library_directory.png)

### tinyobjloader

Tinyobjloader 可以通过 vcpkg 安装：`vcpkg install tinyobjloader`

### 配置 Visual Studio（Setting up Visual Studio）

#### 配置 CMake 项目

现在你已经安装好了所有依赖，我们可以为 Vulkan 创建一个基本的 CMake 项目，并编写少量代码来验证一切是否正常运行。

我们假设你已经具备一些基本的 CMake 使用经验，例如了解变量和规则的工作方式。如果没有，你可以通过相关教程快速上手。

现在你可以使用 `attachment` 文件夹中任意章节的代码作为 Vulkan 项目的模板。复制一份，将其重命名为类似 `HelloTriangle` 的名称，并删除 `main.cpp` 中的所有代码。

恭喜，你已经准备好开始体验 Vulkan 了！

---

## Linux

这些说明主要面向 Ubuntu、Fedora 和 Arch Linux 用户，但你应该可以通过将包管理器专用命令替换为适合你发行版的命令来跟随操作。

你应该有一个支持 C++20 的编译器（GCC 7+ 或 Clang 5+）。你还需要 cmake，大多数内容可以通过 `build-essentials` 等较大的软件包安装。

我们建议使用 CLion 或其他 IDE；但与 Linux 上大多数事情一样，GUI 完全是可选的。

### Vulkan tarball

在 Linux 上开发 Vulkan 应用程序最重要的部分是 Vulkan loader、validation layer，以及几个用于测试你的机器是否支持 Vulkan 的命令行工具：

从 LunarG 下载 VulkanSDK tarball。

将解压后的 VulkanSDK 放置在一个方便的路径下，并创建一个指向最新版本的符号链接，如下所示：

```bash
pushd vulkansdk
tar -xf vulkansdk-linux-x86_64-1.4.304.1.tar.xz
ln -s 1.4.304.1 default
```

然后在你的 `~/.bashrc` 文件中添加以下内容，以便 Vulkan 的环境变量在所有地方生效：

```bash
source ~/vulkanSDK/default/setup-env.sh
```

如果安装成功，Vulkan 部分就应该全部准备好了。

记得运行 `vkcube`，确保看到以下窗口弹出：

![cube demo (no window)](https://docs.vulkan.org/tutorial/latest/_images/images/cube_demo_nowindow.png)

如果收到错误消息，请确保你的驱动程序是最新版本，包含 Vulkan runtime，并且你的显卡受到支持。有关各主要厂商驱动程序的链接，请参阅简介章节。

### Ninja

Ninja 是一个快速构建系统，CMake 在所有平台上都支持它。我们建议通过以下命令安装：

```bash
sudo apt install ninja-build
```

### X Window System 与 XFree86-VidModeExtension

这些库可能不在系统中，如果缺失，可以使用以下命令安装：

- `sudo apt install libxxf86vm-dev` 或 `dnf install libXxf86vm-devel`：提供 XFree86-VidModeExtension 接口。
- `sudo apt install libxi-dev` 或 `dnf install libXi-devel`：提供 XINPUT 扩展的 X Window System 客户端接口。

### GLFW

通过以下命令安装 GLFW：

```bash
sudo apt install libglfw3-dev
```

或

```bash
sudo dnf install glfw-devel
```

或

```bash
sudo pacman -S glfw-wayland # X11 用户请使用 glfw-x11
```

### GLM

GLM 是一个仅有头文件的库，可以从 `libglm-dev` 或 `glm-devel` 包中安装：

```bash
sudo apt install libglm-dev
```

或

```bash
sudo dnf install glm-devel
```

或

```bash
sudo pacman -S glm
```

### 配置 CLion（可选）

你可以从官网获取 CLion。我们建议通过 JetBrains Toolbox 安装，以便自动保持 CLion 更新。

要使用 CLion 这样的 IDE，我们必须设置那些原本由以下命令在终端中配置的环境变量：

```bash
source ~/vulkanSDK/default/setup-env.sh
```

为此，打开 Settings，选择 "Build, Execution, Deployment"，然后选择 CMake。在该窗口底部可以找到环境变量设置。在那里添加 `VULKAN_SDK=<fullPathToVulkanSDK>`，Vulkan 就会在编译时被找到。注意，完整路径并不仅仅是顶层的 VulkanSDK 文件夹。在终端运行以下命令查找确切路径：

```bash
echo $VULKAN_SDK
```

为了方便起见，至少对于运行时，我们建议将 layer 放置在系统范围内。为此，在终端中执行以下命令：

```bash
sudo cp $VULKAN_SDK/lib/libVkLayer_*.so /usr/local/lib/
sudo mkdir -p /usr/local/share/vulkan/explicit_layer.d
sudo cp $VULKAN_SDK/share/vulkan/explicit_layer.d/VkLayer_*.json /usr/local/share/vulkan/explicit_layer.d
```

另外，你可以将 `VK_LAYER_PATH` 添加到系统环境变量中，并将其指向 `$VULKAN_SDK/share/vulkan/explicit_layer.d`。同时，你还需要将 `$VULKAN_SDK/lib` 路径添加到 `LD_LIBRARY_CONFIG` 中。当使用终端时，`setup-env.sh` 文件会为你完成这一切。

### 配置 CMake 项目

现在你已经安装好了所有依赖，我们可以为 Vulkan 创建一个基本的 CMake 项目，并编写少量代码来验证一切是否正常运行。

我们假设你已经具备一些基本的 CMake 使用经验，例如了解变量和规则的工作方式。如果没有，你可以通过相关教程快速上手。

现在你可以使用本教程的 `attachments` 目录作为 Vulkan 项目的模板。复制一份，将其重命名为类似 `HelloTriangle` 的名称，并删除 `main.cpp` 中的所有代码。

你现在已经为真正的冒险做好了准备。

---

## macOS

这些说明假设你正在使用 Xcode 和 Homebrew 包管理器。另外，请注意你需要至少 macOS 10.11 版本，且你的设备需要支持 Metal API。

### Vulkan SDK

macOS 版本的 SDK 内部使用 MoltenVK。macOS 上没有对 Vulkan 的原生支持，MoltenVK 的作用实际上是充当一个将 Vulkan API 调用转换为 Apple 的 Metal 图形框架的中间层。通过这种方式，你可以利用 Apple Metal 框架的调试和性能优势。

下载 macOS 的安装程序后，双击安装程序并按照提示操作。在"Installation Folder"步骤中记下安装位置，在 Xcode 中创建项目时需要用到它。

**注意**：在本教程中，`vulkansdk` 指代你安装 VulkanSDK 的路径。

![SDK install on Mac](https://docs.vulkan.org/tutorial/latest/_images/images/sdk_install_mac.png)

在 `vulkansdk/Applications` 文件夹中，你应该有一些可运行 SDK 演示的可执行文件。运行 `vkcube` 可执行文件，你将看到以下画面：

![cube demo (Mac)](https://docs.vulkan.org/tutorial/latest/_images/images/cube_demo_mac.png)

### GLFW

要在 macOS 上安装 GLFW，我们将使用 Homebrew 包管理器获取 `glfw` 包：

```bash
brew install glfw
```

### GLM

GLM 是一个仅有头文件的库，可以从 `glm` 包中安装：

```bash
brew install glm
```

### 配置 Xcode（Setting up Xcode）

现在所有依赖都已安装，我们可以为 Vulkan 设置一个基本的 Xcode 项目了。这里的大部分说明本质上是大量的"配管工作"，以便将所有依赖链接到项目中。另外请记住，在以下说明中，每当我们提到文件夹 `vulkansdk` 时，都是指你解压 Vulkan SDK 的文件夹。

我们建议在所有地方都使用 CMake，Apple 平台也不例外。关于如何在 Apple 平台使用 CMake 的示例可以在相关文档中找到。

我们在 VulkanSamples 项目中也有关于在 Apple 环境中使用 cmake 项目的文档，该文档同时针对 iOS 和桌面 Apple 平台。

使用 CMake 配合 XCode generator 后，打开生成的 Xcode 项目。如果你使用本教程的 `code` 目录，可以在命令行中执行以下操作：

```bash
cd code
cmake -G XCode
```

最后，你需要设置几个环境变量。在 Xcode 工具栏中，进入 Product > Scheme > Edit Scheme...，在 Arguments 标签页中添加以下两个环境变量：

```
VK_ICD_FILENAMES = vulkansdk/macOS/share/vulkan/icd.d/MoltenVK_icd.json
VK_LAYER_PATH = vulkansdk/macOS/share/vulkan/explicit_layer.d
```

取消勾选"shared"，效果如下所示：

![Xcode environment variables](https://docs.vulkan.org/tutorial/latest/_images/images/xcode_variables.png)

最后，一切就绪！

你现在已经为真正的开始做好了准备。

---

## Android

Vulkan 是 Android 上的一等 API，且被广泛支持。但使用它在窗口管理、构建系统等多个关键方面与桌面平台有所不同。因此，虽然基础章节侧重于桌面平台，本教程也有一个专门的章节，将引导你完成开发环境的搭建，并让教程代码在 Android 上运行起来。
