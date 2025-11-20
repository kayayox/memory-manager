#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/memory_pool.h"
#include "../include/memory_client.h"
#include "../include/memory_metrics.h"

void test_strategy_comparison() {
    printf("=== COMPARACIÓN DE ESTRATEGIAS DE ASIGNACIÓN ===\n");

    alloc_strategy_t strategies[] = {ALLOC_FIRST_FIT, ALLOC_BEST_FIT, ALLOC_WORST_FIT, ALLOC_NEXT_FIT};
    const char* strategy_names[] = {"FIRST_FIT", "BEST_FIT", "WORST_FIT", "NEXT_FIT"};

    for (int s = 0; s < 4; s++) {
        printf("\n--- Probando estrategia: %s ---\n", strategy_names[s]);

        memory_pool_t* pool = memory_pool_create(1024 * 1024, strategies[s]);
        memory_client_t* client = memory_client_create(1, pool);

        if (!pool || !client) {
            printf("Error al crear pool o cliente\n");
            continue;
        }

        // Patrón de asignación mixto
        void* blocks[10];
        size_t sizes[] = {128, 256, 512, 1024, 64, 384, 768, 192, 896, 320};
        int successful_allocations = 0;

        for (int i = 0; i < 10; i++) {
            blocks[i] = memory_client_alloc(client, sizes[i]);
            if (blocks[i]) {
                successful_allocations++;
            } else {
                printf("  Falló asignación de %zu bytes\n", sizes[i]);
            }
        }

        printf("  Asignaciones exitosas: %d/10\n", successful_allocations);

        // Verificar integridad antes de liberar
        if (memory_pool_check(pool)!=0) {
            printf("  ADVERTENCIA: Pool corrupto antes de liberaciones\n");
        }

        // Liberar algunos bloques estratégicamente
        int freed_count = 0;
        int indices_to_free[] = {2, 5, 7};
        for (int i = 0; i < 3; i++) {
            if (blocks[indices_to_free[i]]) {
                if (memory_client_free(client, blocks[indices_to_free[i]]) == MEMORY_SUCCESS) {
                    freed_count++;
                    blocks[indices_to_free[i]] = NULL;
                }
            }
        }

        printf("  Bloques liberados: %d/3\n", freed_count);

        // Nueva asignación que debería usar los huecos
        void* new_block = memory_client_alloc(client, 400);
        if (new_block) {
            printf("  Asignación de 400 bytes después de liberaciones: EXITOSA\n");
            memory_client_free(client, new_block);
        } else {
            printf("  Asignación de 400 bytes después de liberaciones: FALLÓ\n");
        }

        pool_metrics_t metrics;
        memory_pool_get_metrics(pool, &metrics);

        printf("  Bloques usados: %d, Fragmentación: %.1f%%, Mayor bloque libre: %zu bytes\n",
               metrics.used_blocks, metrics.fragmentation, metrics.largest_free_block);

        // Verificar integridad final
        if (memory_pool_check(pool)!=0) {
            printf("  ADVERTENCIA: Pool corrupto al final de la prueba\n");
        }

        memory_client_destroy(client);
        memory_pool_destroy(pool);
    }
}

void test_fragmentation_scenario() {
    printf("\n=== ESCENARIO DE FRAGMENTACIÓN ===\n");

    memory_pool_t* pool = memory_pool_create(1024 * 1024, ALLOC_FIRST_FIT);
    memory_client_t* client = memory_client_create(1, pool);

    if (!pool || !client) {
        printf("Error al crear pool o cliente\n");
        return;
    }

    // Crear fragmentación intencional
    void* small_blocks[20];
    printf("Creando 20 bloques pequeños...\n");
    int allocated_count = 0;

    for (int i = 0; i < 20; i++) {
        small_blocks[i] = memory_client_alloc(client, 64);
        if (small_blocks[i]) {
            allocated_count++;
        }
    }
    printf("  Bloques pequeños creados: %d/20\n", allocated_count);

    // Verificar estado antes de liberar
    if (memory_pool_check(pool)!=0) {
        printf("ADVERTENCIA: Pool corrupto antes de liberaciones\n");
    }

    // Liberar bloques alternados para crear fragmentación
    printf("Liberando bloques alternados...\n");
    int freed_count = 0;
    for (int i = 0; i < 20; i += 2) {
        if (small_blocks[i]) {
            if (memory_client_free(client, small_blocks[i]) == MEMORY_SUCCESS) {
                freed_count++;
                small_blocks[i] = NULL;
            }
        }
    }
    printf("  Bloques liberados: %d\n", freed_count);

    pool_metrics_t frag_metrics;
    memory_pool_get_metrics(pool, &frag_metrics);

    printf("Estado después de fragmentación:\n");
    printf("  Bloques libres: %d, Fragmentación: %.1f%%, Mayor bloque libre: %zu bytes\n",
           frag_metrics.free_blocks, frag_metrics.fragmentation, frag_metrics.largest_free_block);

    // Verificar integridad antes de asignación grande
    if (memory_pool_check(pool)!=0) {
        printf("ADVERTENCIA: Pool corrupto antes de asignación grande\n");
    }

    // Intentar asignar un bloque grande
    printf("Intentando asignar bloque de 2000 bytes...\n");
    void* large_block = memory_client_alloc(client, 2000);
    if (large_block) {
        printf("    Asignación exitosa a pesar de la fragmentación\n");
        memory_client_free(client, large_block);
    } else {
        printf("    No se pudo asignar debido a fragmentación\n");
        memory_pool_print_metrics(pool);
    }

    // Liberar bloques restantes
    for (int i = 1; i < 20; i += 2) {
        if (small_blocks[i]) {
            memory_client_free(client, small_blocks[i]);
        }
    }

    memory_client_destroy(client);
    memory_pool_destroy(pool);
}

int main() {
    printf("Iniciando pruebas detalladas...\n");

    test_strategy_comparison();
    test_fragmentation_scenario();

    printf("\n=== Todas las pruebas completadas ===\n");
    return 0;
}
