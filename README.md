# vulkan_tutorial

基于 Vulkan 1.3 的图形学习项目，使用 Vulkan-Hpp RAII 封装和 GLFW 窗口管理。

## 特性

- Vulkan 1.3 + Dynamic Rendering + Extended Dynamic State
- Vulkan-Hpp RAII (`vulkan_raii.hpp`)
- Validation Layer 支持（Debug 模式自动启用）
- C++20

## 当前进度

**Drawing a Triangle — Setup**

- [x] Base code / 窗口初始化
- [x] Instance 创建（扩展检查、Layer 检查）
- [x] Validation Layers / Debug Messenger
- [x] Physical Device 选择（Vulkan 1.3、Dynamic Rendering、Extended Dynamic State 特性检查）
- [x] Logical Device + Graphics Queue
- [x] Window Surface
- [x] Swap Chain
- [x] Image Views
- [ ] Graphics Pipeline
- [ ] ...

## 依赖

- [Vulkan SDK](https://vulkan.lunarg.com/)（需预先安装）
- [GLFW 3.4](https://github.com/glfw/glfw)
- [GLM 1.0.1](https://github.com/g-truc/glm)
- [tinyobjloader v2.0.0rc13](https://github.com/tinyobjloader/tinyobjloader)

GLFW、GLM、tinyobjloader 由 `setup.bat` 自动下载和编译，无需手动安装。

## 构建

```bat
:: 1. 下载并编译第三方库
setup.bat

:: 2. 构建项目（默认 Debug）
build.bat

:: 构建 Release
build.bat Release
```

## 清理

```bat
:: 清除 build、bin、thirdparty 目录
clean.bat
```

## 项目结构

```
├── src/              # 源代码
├── include/          # 头文件
├── doc/              # 教程笔记（Markdown）
├── thirdparty/       # 第三方库（自动生成）
├── build/            # CMake 构建目录
├── bin/              # 输出可执行文件
├── setup.bat         # 下载编译第三方库
├── build.bat         # 构建项目
├── clean.bat         # 清理编译产出
└── CMakeLists.txt
```
