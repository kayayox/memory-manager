#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/memory_pool.h"
#include "../include/memory_client.h"
#include "../include/memory_metrics.h"

int main() {
    printf("=== EJEMPLO BÁSICO USO DE MEMORY MANAGER ===\n");

    // Crear pool con estrategia FIRST_FIT
    memory_pool_t* pool = memory_pool_create(1024 * 1024, ALLOC_FIRST_FIT);
    if (!pool) {
        printf("Error al crear pool\n");
        return 1;
    }

    // Crear clientes
    memory_client_t* client1 = memory_client_create(1, pool);
    memory_client_t* client2 = memory_client_create(2, pool);

    if (!client1 || !client2) {
        printf("Error al crear clientes\n");
        memory_pool_destroy(pool);
        return 1;
    }

    // Asignaciones de diferentes tipos
    printf("\n--- Realizando asignaciones ---\n");

    int* numbers = memory_client_alloc(client1, sizeof(int) * 100);
    char* text = memory_client_alloc(client2, 256);
    double* values = memory_client_alloc(client1, sizeof(double) * 50);

    if (numbers) {
        for (int i = 0; i < 100; i++) numbers[i] = i * 2;
        printf("Client 1: Array de enteros inicializado\n");
    }

    if (text) {
        strcpy(text, "Hola desde el Memory Manager!");
        printf("Client 2: Texto asignado: %s\n", text);
    }

    if (values) {
        for (int i = 0; i < 50; i++) values[i] = i * 3.14;
        printf("Client 1: Array de doubles inicializado\n");
    }

    // Mostrar métricas
    memory_pool_print_metrics(pool);

    // Liberar algunos bloques
    printf("\n--- Liberando memoria ---\n");
    if (text) {
        memory_client_free(client2, text);
        printf("Client 2: Texto liberado\n");
    }

    // Mostrar métricas después de liberar
    memory_pool_print_metrics(pool);

    // Cambiar estrategia
    printf("\n--- Cambiando a estrategia BEST_FIT ---\n");
    memory_pool_set_strategy(pool, ALLOC_BEST_FIT);

    // Nueva asignación con nueva estrategia
    float* new_data = memory_client_alloc(client2, sizeof(float) * 200);
    if (new_data) {
        for (int i = 0; i < 200; i++) new_data[i] = i * 1.5f;
        printf("Client 2: Nuevo array con BEST_FIT\n");
    }

    // Métricas finales
    memory_pool_print_metrics(pool);

    // Verificar integridad
    printf("\n--- Verificando integridad ---\n");
    if (memory_pool_check(pool)) {
        printf("✓ Integridad del pool verificada\n");
    } else {
        printf("✗ Problemas de integridad detectados\n");
    }

    // Limpieza
    printf("\n--- Limpiando recursos ---\n");
    memory_client_destroy(client1);
    memory_client_destroy(client2);
    memory_pool_destroy(pool);

    printf("=== Ejemplo completado ===\n");
    return 0;
}
