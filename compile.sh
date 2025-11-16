#!/bin/bash

# Configuración
BUILD_DIR="build"
LIB_NAME="memory_manager"
EXAMPLES=("basic_usage")

# Crear directorio de build
mkdir -p $BUILD_DIR

# Compilar librería
echo "Compilando librería..."
gcc -c -Iinclude -std=c11 -Wall -Wextra -pthread \
    src/memory_pool.c -o $BUILD_DIR/memory_pool.o
gcc -c -Iinclude -std=c11 -Wall -Wextra -pthread \
    src/memory_client.c -o $BUILD_DIR/memory_client.o
gcc -c -Iinclude -std=c11 -Wall -Wextra -pthread \
    src/memory_metrics.c -o $BUILD_DIR/memory_metrics.o

# Crear librería estática
echo "Creando librería estática..."
ar rcs $BUILD_DIR/lib$LIB_NAME.a \
    $BUILD_DIR/memory_pool.o \
    $BUILD_DIR/memory_client.o \
    $BUILD_DIR/memory_metrics.o

# Compilar ejemplos
for example in "${EXAMPLES[@]}"; do
    echo "Compilando ejemplo: $example"
    gcc -Iinclude -std=c11 -Wall -Wextra -pthread \
        examples/$example.c -L$BUILD_DIR -l$LIB_NAME \
        -o $BUILD_DIR/$example
done

echo "Compilación completada!"
echo "Librería: $BUILD_DIR/lib$LIB_NAME.a"
echo "Ejemplos: $BUILD_DIR/basic_usage"
