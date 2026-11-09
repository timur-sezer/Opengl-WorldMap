cmake_minimum_required(VERSION 3.12)
project(Opengl-WorldMap)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Set C++ standard to C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- 1. Find Required Packages ---
find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(CURL REQUIRED)     # downloads map tiles
find_package(Threads REQUIRED)  # tile download workers


# --- 2. GLAD (bundled in dependencies/, works both natively and in Docker) ---
add_library(glad STATIC dependencies/src/glad.c)
target_include_directories(glad PUBLIC dependencies/include)

# --- 3. ImGui ---
add_library(imgui STATIC
        dependencies/imgui/imgui.cpp
        dependencies/imgui/imgui_tables.cpp
        dependencies/imgui/imgui_draw.cpp
        dependencies/imgui/imgui_demo.cpp
        dependencies/imgui/imgui_widgets.cpp
        dependencies/imgui/imgui_impl_glfw.cpp
        dependencies/imgui/imgui_impl_opengl3.cpp
)
target_include_directories(imgui SYSTEM PUBLIC dependencies/imgui)  # SYSTEM: hide warnings from ImGui headers
target_link_libraries(imgui PUBLIC glfw OpenGL::GL)

# --- 4. Executable ---
add_executable(Opengl-WorldMap
        main.cpp
        Map/TileManager.cpp
        Map/TileManager.h
        Map/MapWidget.cpp
        Map/MapWidget.h
        Theme/Theme.cpp
        Theme/Theme.h
)

# Absolute asset path so the program runs from any working directory
target_compile_definitions(Opengl-WorldMap PRIVATE WORLDMAP_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets")

target_link_libraries(Opengl-WorldMap PRIVATE glad imgui glfw OpenGL::GL CURL::libcurl Threads::Threads ${CMAKE_DL_LIBS})


# --- 5. Compile Options ---
if(NOT MSVC)
    target_compile_options(Opengl-WorldMap PRIVATE -Wall -Wextra)
else()
    target_compile_options(Opengl-WorldMap PRIVATE -W2)
endif()
