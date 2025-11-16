cmake_minimum_required(VERSION 3.10)
project(MemoryManager VERSION 1.0.0 LANGUAGES C)

# Configuración del proyecto
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Opciones de compilación
option(MEMORY_DEBUG "Habilitar modo debug" OFF)
option(BUILD_EXAMPLES "Compilar ejemplos" ON)
option(BUILD_TESTS "Compilar tests" OFF)

# Directorios
include_directories(include)
set(SOURCES_DIR src)

# Librería principal
set(MEMORY_SOURCES
    ${SOURCES_DIR}/memory_pool.c
    ${SOURCES_DIR}/memory_client.c
    ${SOURCES_DIR}/memory_metrics.c
)

add_library(memory_manager STATIC ${MEMORY_SOURCES})

# Flags de compilación
target_compile_definitions(memory_manager PRIVATE
    $<$<BOOL:${MEMORY_DEBUG}>:MEMORY_DEBUG>
)

target_compile_options(memory_manager PRIVATE
    -Wall -Wextra -Wpedantic
)

# Enlazar pthreads
find_package(Threads REQUIRED)
target_link_libraries(memory_manager PRIVATE Threads::Threads)

# Ejemplos
if(BUILD_EXAMPLES)
    add_executable(basic_example examples/basic_usage.c)
    target_link_libraries(basic_example memory_manager)

    set_target_properties(basic_example PROPERTIES
        OUTPUT_NAME "memory_example"
    )
endif()

# Instalación
install(TARGETS memory_manager
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/ DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

# Configuración de paquete
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    MemoryManagerConfigVersion.cmake
    VERSION ${PACKAGE_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Exportación
export(EXPORT MemoryManagerTargets
    FILE "${CMAKE_CURRENT_BINARY_DIR}/MemoryManagerTargets.cmake"
)

configure_package_config_file(
    cmake/MemoryManagerConfig.cmake.in
    MemoryManagerConfig.cmake
    INSTALL_DESTINATION lib/cmake/MemoryManager
)

install(
    FILES
    ${CMAKE_CURRENT_BINARY_DIR}/MemoryManagerConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/MemoryManagerConfigVersion.cmake
    DESTINATION lib/cmake/MemoryManager
)

install(
    EXPORT MemoryManagerTargets
    FILE MemoryManagerTargets.cmake
    DESTINATION lib/cmake/MemoryManager
)
