
cmake_minimum_required(VERSION 3.16)
project(vk_autotune LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(Vulkan REQUIRED)

# Shader paths
set(SHADER_SRC ${CMAKE_SOURCE_DIR}/shaders/gemm.comp)
set(SHADER_DST ${CMAKE_BINARY_DIR}/shaders/gemm.spv)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/shaders)

# Prefer glslc; fallback to glslangValidator
find_program(GLSLC glslc)
set(COMPILE_TOOL "")
if (GLSLC)
  set(COMPILE_TOOL "glslc")
  add_custom_command(
    OUTPUT ${SHADER_DST}
    COMMAND ${GLSLC} -O -fshader-stage=compute ${SHADER_SRC} -o ${SHADER_DST}
    DEPENDS ${SHADER_SRC}
    COMMENT "Compiling gemm.comp to SPIR-V with glslc"
  )
else()
  find_program(GLSLANGVALIDATOR glslangValidator)
  if (GLSLANGVALIDATOR)
    set(COMPILE_TOOL "glslangValidator")
    add_custom_command(
      OUTPUT ${SHADER_DST}
      COMMAND ${GLSLANGVALIDATOR} -V -S comp ${SHADER_SRC} -o ${SHADER_DST}
      DEPENDS ${SHADER_SRC}
      COMMENT "Compiling gemm.comp to SPIR-V with glslangValidator"
    )
  else()
    message(FATAL_ERROR "Neither glslc nor glslangValidator found. Install glslang-tools or shaderc-tools.")
  endif()
endif()

add_custom_target(spv-build DEPENDS ${SHADER_DST})

add_executable(autotune main.cpp)
add_dependencies(autotune spv-build)
target_include_directories(autotune PRIVATE ${Vulkan_INCLUDE_DIRS})
target_link_libraries(autotune PRIVATE ${Vulkan_LIBRARIES})

# Export the chosen shader compiler name into the binary
target_compile_definitions(autotune PRIVATE VK_AT_COMPILE_TOOL="${COMPILE_TOOL}")
