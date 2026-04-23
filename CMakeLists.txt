cmake_minimum_required(VERSION 3.10)
project(ExtreamFS)

# Estándar de C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Directorio de headers
include_directories(${CMAKE_SOURCE_DIR}/backend/include)

# Crear la librería del backend
add_library(extremsfs_lib
    backend/src/disk_manager.cpp
    backend/src/command_parser.cpp
    backend/src/command_handler.cpp
)

# Crear el ejecutable
add_executable(extremsfs
    backend/src/main.cpp
)

# Enlazar librería
target_link_libraries(extremsfs PRIVATE extremsfs_lib)

# Flags de compilación
if(UNIX AND NOT APPLE)
    target_compile_options(extremsfs PRIVATE -Wall -Wextra -O2)
endif()

# Crear directorio de salida
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
