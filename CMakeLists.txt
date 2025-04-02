cmake_minimum_required(VERSION 3.10)
project(GrafikaProject)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include könyvtárak beállítása
include_directories(
        glProgram/sources
        Libraries/Glad/include
        Libraries/glm/include
)

# GLAD beállítása (mivel a glad.c nincs automatikusan linkelve)
add_library(glad glProgram/sources/glad.c)
target_include_directories(glad PUBLIC Libraries/Glad/include)

# OpenGL és GLFW keresése
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

# Fő program (GreenTriangle.cpp a belépési pont)
add_executable(GrafikaProject
        glProgram/sources/framework.cpp
        #glProgram/sources/greenTriangle.cpp
        #glProgram/sources/pointsLines.cpp
        glProgram/sources/rollercoaster.cpp
)

# Külső könyvtárak linkelése
target_link_libraries(GrafikaProject PRIVATE glfw glad OpenGL::GL)