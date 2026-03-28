cmake_minimum_required(VERSION 3.14)
project(vulkan_tutorial)

set(CMAKE_CXX_STANDARD 20)

# ---------- output directories ----------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# ---------- project sources ----------
file(GLOB_RECURSE SOURCES "${CMAKE_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE HEADERS "${CMAKE_SOURCE_DIR}/include/*.h")

add_executable(vulkan_tutorial ${SOURCES} ${HEADERS})

target_include_directories(vulkan_tutorial PRIVATE ${CMAKE_SOURCE_DIR}/include)

# ---------- Vulkan ----------
find_package(Vulkan REQUIRED)
target_link_libraries(vulkan_tutorial PRIVATE Vulkan::Vulkan)
target_compile_definitions(vulkan_tutorial PRIVATE VULKAN_HPP_NO_CONSTRUCTORS)

# ---------- GLFW (pre-built in thirdparty) ----------
set(GLFW_DIR ${CMAKE_SOURCE_DIR}/thirdparty/glfw)
target_include_directories(vulkan_tutorial PRIVATE ${GLFW_DIR}/include)
target_link_libraries(vulkan_tutorial PRIVATE
    $<$<CONFIG:Debug>:${GLFW_DIR}/lib/Debug/glfw3.lib>
    $<$<CONFIG:Release>:${GLFW_DIR}/lib/Release/glfw3.lib>
    $<$<CONFIG:RelWithDebInfo>:${GLFW_DIR}/lib/Release/glfw3.lib>
    $<$<CONFIG:MinSizeRel>:${GLFW_DIR}/lib/Release/glfw3.lib>
)

# ---------- GLM (header-only in thirdparty) ----------
set(GLM_DIR ${CMAKE_SOURCE_DIR}/thirdparty/glm)
target_include_directories(vulkan_tutorial PRIVATE ${GLM_DIR}/include)

# ---------- tinyobjloader (header-only in thirdparty) ----------
set(TINYOBJ_DIR ${CMAKE_SOURCE_DIR}/thirdparty/tinyobjloader)
target_include_directories(vulkan_tutorial PRIVATE ${TINYOBJ_DIR}/include)
