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

# Demo valasztas: csak ezt a nevet kell modositani
set(DEMO mercator CACHE STRING "Demo name: mercator, pointsLines, rollercoaster, hengerekRay")
set(DEMO_SOURCE "glProgram/sources/${DEMO}.cpp")

if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${DEMO_SOURCE}")
        message(FATAL_ERROR "Unknown demo '${DEMO}'. Valid: mercator, pointsLines, rollercoaster, hengerekRay")
endif()

# Fő program
add_executable(GrafikaProject
        glProgram/sources/framework.cpp
        ${DEMO_SOURCE}
)

# Forditas + futtatas egy paranccsal
add_custom_target(run
        COMMAND $<TARGET_FILE:GrafikaProject>
        DEPENDS GrafikaProject
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Külső könyvtárak linkelése
target_link_libraries(GrafikaProject PRIVATE glfw glad OpenGL::GL)