cmake_minimum_required(VERSION 3.16)
project(BloomVision LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 3.3 REQUIRED)

add_executable(bloomvision
  src/main.cpp
  src/Shader.cpp
  src/PageTexture.cpp)

target_link_libraries(bloomvision PRIVATE OpenGL::GL GLEW::GLEW glfw)
target_compile_options(bloomvision PRIVATE -Wall -Wextra)

# Ruta por defecto de los shaders (se puede cambiar en ejecución con --shaders)
target_compile_definitions(bloomvision PRIVATE
  DEFAULT_SHADER_DIR="${CMAKE_SOURCE_DIR}/shaders")
