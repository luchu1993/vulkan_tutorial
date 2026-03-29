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
target_compile_definitions(vulkan_tutorial PRIVATE VULKAN_HPP_NO_STRUCT_CONSTRUCTORS)

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

# ---------- Slang shader compilation ----------
find_program(SLANGC_EXECUTABLE slangc REQUIRED)

function (add_slang_shader_target TARGET)
  cmake_parse_arguments ("SHADER" "" "" "SOURCES" ${ARGN})
  set (SHADERS_DIR ${CMAKE_CURRENT_LIST_DIR}/shaders)
  set (ENTRY_POINTS -entry vertMain -entry fragMain)
  add_custom_command (
          OUTPUT  ${SHADERS_DIR}/slang.spv
          COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADERS_DIR}
          COMMAND ${SLANGC_EXECUTABLE} ${SHADER_SOURCES} -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name ${ENTRY_POINTS} -o slang.spv
          WORKING_DIRECTORY ${SHADERS_DIR}
          DEPENDS ${SHADER_SOURCES}
          COMMENT "Compiling Slang Shaders"
          VERBATIM
  )
  add_custom_target (${TARGET} DEPENDS ${SHADERS_DIR}/slang.spv)
endfunction()

add_slang_shader_target(shaders SOURCES ${CMAKE_SOURCE_DIR}/shaders/shader.slang)
add_dependencies(vulkan_tutorial shaders)
