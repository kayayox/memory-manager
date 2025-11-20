#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/memory_pool.h"
#include "../include/memory_client.h"
#include "../include/memory_metrics.h"

void test_next_fit_specific() {
    printf("=== TEST ESPECÍFICO NEXT_FIT ===\n");

    memory_pool_t* pool = memory_pool_create(1024 * 1024, ALLOC_NEXT_FIT);
    memory_client_t* client = memory_client_create(1, pool);

    if (!pool || !client) {
        printf("Error al crear pool o cliente\n");
        if (pool) memory_pool_destroy(pool);
        return;
    }

    printf("Pool creado con estrategia NEXT_FIT\n");

    // Asignación inicial
    void* block1 = memory_client_alloc(client, 100);
    void* block2 = memory_client_alloc(client, 200);
    void* block3 = memory_client_alloc(client, 300);

    printf("Asignaciones iniciales:\n");
    printf("  Block1: %p (%s)\n", block1, block1 ? "EXITOSO" : "FALLIDO");
    printf("  Block2: %p (%s)\n", block2, block2 ? "EXITOSO" : "FALLIDO");
    printf("  Block3: %p (%s)\n", block3, block3 ? "EXITOSO" : "FALLIDO");

    // Liberar el segundo bloque
    if (block2) {
        memory_client_free(client, block2);
        printf("Block2 liberado\n");
    }

    // Nueva asignación - debería usar NEXT_FIT desde la posición actual
    void* block4 = memory_client_alloc(client, 150);
    printf("Nueva asignación después de liberar:\n");
    printf("  Block4: %p (%s)\n", block4, block4 ? "EXITOSO" : "FALLIDO");

    // Más asignaciones para probar el comportamiento circular
    void* block5 = memory_client_alloc(client, 400);
    void* block6 = memory_client_alloc(client, 250);

    printf("Asignaciones adicionales:\n");
    printf("  Block5: %p (%s)\n", block5, block5 ? "EXITOSO" : "FALLIDO");
    printf("  Block6: %p (%s)\n", block6, block6 ? "EXITOSO" : "FALLIDO");

    // Métricas finales
    pool_metrics_t metrics;
    memory_pool_get_metrics(pool, &metrics);

    printf("\nMétricas finales NEXT_FIT:\n");
    printf("  Asignaciones exitosas: %zu\n", metrics.allocation_count - metrics.failed_allocations);
    printf("  Asignaciones fallidas: %zu\n", metrics.failed_allocations);
    printf("  Fragmentación: %.1f%%\n", metrics.fragmentation);

    memory_client_destroy(client);
    memory_pool_destroy(pool);
}

void test_next_fit_circular() {
    printf("\n=== TEST COMPORTAMIENTO CIRCULAR NEXT_FIT ===\n");

    memory_pool_t* pool = memory_pool_create(1024 * 1024, ALLOC_NEXT_FIT);
    memory_client_t* client = memory_client_create(1, pool);

    if (!pool || !client) {
        printf("Error al crear pool o cliente\n");
        if (pool) memory_pool_destroy(pool);
        return;
    }

    // Crear múltiples bloques pequeños
    void* blocks[10];
    for (int i = 0; i < 10; i++) {
        blocks[i] = memory_client_alloc(client, 50);
        printf("Asignación %d: %p\n", i, blocks[i]);
    }

    // Liberar bloques alternados
    for (int i = 0; i < 10; i += 2) {
        if (blocks[i]) {
            memory_client_free(client, blocks[i]);
            printf("Liberado bloque %d\n", i);
        }
    }

    // Intentar asignaciones que deberían usar el comportamiento circular
    printf("Asignaciones con comportamiento circular:\n");
    for (int i = 0; i < 5; i++) {
        void* new_block = memory_client_alloc(client, 40);
        printf("  Nueva asignación %d: %p\n", i, new_block);
    }

    memory_client_destroy(client);
    memory_pool_destroy(pool);
}

int main() {
    srand((unsigned int)time(NULL));
    test_next_fit_specific();
    test_next_fit_circular();
    return 0;
}
