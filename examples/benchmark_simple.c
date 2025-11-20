#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/resource.h>
#include "../include/memory_pool.h"
#include "../include/memory_client.h"
#include "../include/memory_metrics.h"

#define NUM_OPERATIONS 2000
#define MAX_BLOCK_SIZE 512

// Función para obtener el uso de memoria en KB
size_t get_memory_usage() {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_maxrss;
}

// Benchmark con nuestro memory manager
void benchmark_custom_memory_manager() {
    printf("=== BENCHMARK MEMORY MANAGER PERSONALIZADO ===\n");

    clock_t start_time = clock();
    size_t start_memory = get_memory_usage();

    memory_pool_t* pool = memory_pool_create(20 * 1024 * 1024, ALLOC_FIRST_FIT);
    memory_client_t* client = memory_client_create(1, pool);

    if (!pool || !client) {
        printf("Error inicializando memory manager\n");
        if (pool) memory_pool_destroy(pool);
        return;
    }

    void* blocks[NUM_OPERATIONS];
    size_t sizes[NUM_OPERATIONS];
    int allocations_successful = 0;

    printf("Fase 1: Asignando %d bloques...\n", NUM_OPERATIONS);
    for (int i = 0; i < NUM_OPERATIONS; i++) {
        sizes[i] = MIN_BLOCK_SIZE + (rand() % (MAX_BLOCK_SIZE - MIN_BLOCK_SIZE));
        blocks[i] = memory_client_alloc(client, sizes[i]);

        if (blocks[i]) {
            allocations_successful++;
            memset(blocks[i], 0xAA, sizes[i]);
        }

        if (i % 500 == 0 && i > 0) {
            if (!memory_pool_check(pool)) {
                printf("ERROR: Pool corrupto en iteración %d\n", i);
                break;
            }
        }
    }

    printf("Asignaciones exitosas: %d/%d\n", allocations_successful, NUM_OPERATIONS);

    // Fase 2: Liberaciones aleatorias
    printf("Fase 2: Liberando 30%% de bloques aleatoriamente...\n");
    int blocks_freed = 0;
    for (int i = 0; i < NUM_OPERATIONS / 3; i++) {
        int index = rand() % NUM_OPERATIONS; // ✅ CORREGIDO: Sin +1
        if (blocks[index]) {
            memory_client_free(client, blocks[index]);
            blocks[index] = NULL;
            blocks_freed++;
        }
    }
    printf("Bloques liberados: %d\n", blocks_freed);

    // Fase 3: Re-asignaciones
    printf("Fase 3: Re-asignando bloques liberados...\n");
    int reallocations = 0;
    for (int i = 0; i < NUM_OPERATIONS; i++) {
        if (!blocks[i] && reallocations < blocks_freed) {
            blocks[i] = memory_client_alloc(client, sizes[i]);
            if (blocks[i]) {
                reallocations++;
            }
        }
    }
    printf("Re-asignaciones exitosas: %d/%d\n", reallocations, blocks_freed);

    // Verificar estado final
    pool_metrics_t final_metrics;
    memory_pool_get_metrics(pool, &final_metrics);
    printf("Estado final - Usados: %d, Libres: %d, Fragmentación: %.1f%%\n",
           final_metrics.used_blocks, final_metrics.free_blocks, final_metrics.fragmentation);

    printf("Fase 4: Liberando todos los bloques...\n"); // ✅ CORREGIDO: Sin verificación redundante
    memory_client_destroy(client);
    memory_pool_destroy(pool);

    clock_t end_time = clock();
    size_t end_memory = get_memory_usage();

    double total_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    size_t memory_used = end_memory - start_memory;

    printf("RESULTADOS:\n");
    printf("  Tiempo total: %.4f segundos\n", total_time);
    printf("  Memoria utilizada: %zu KB\n", memory_used);
    printf("  Operaciones por segundo: %.0f\n", NUM_OPERATIONS * 3 / total_time);
}

// Benchmark con malloc/free estándar
void benchmark_standard_malloc() {
    printf("\n=== BENCHMARK MALLOC/FREE ESTÁNDAR ===\n");

    clock_t start_time = clock();
    size_t start_memory = get_memory_usage();

    void* blocks[NUM_OPERATIONS];
    size_t sizes[NUM_OPERATIONS];

    printf("Fase 1: Asignando %d bloques...\n", NUM_OPERATIONS);
    for (int i = 0; i < NUM_OPERATIONS; i++) {
        sizes[i] = MIN_BLOCK_SIZE + (rand() % (MAX_BLOCK_SIZE - MIN_BLOCK_SIZE));
        blocks[i] = malloc(sizes[i]);

        if (blocks[i]) {
            memset(blocks[i], 0xAA, sizes[i]);
        }
    }

    printf("Fase 2: Liberando 30%% de bloques aleatoriamente...\n");
    for (int i = 0; i < NUM_OPERATIONS / 3; i++) {
        int index = rand() % NUM_OPERATIONS;
        if (blocks[index]) {
            free(blocks[index]);
            blocks[index] = NULL;
        }
    }

    printf("Fase 3: Re-asignando bloques liberados...\n");
    for (int i = 0; i < NUM_OPERATIONS; i++) {
        if (!blocks[i]) {
            blocks[i] = malloc(sizes[i]);
        }
    }

    printf("Fase 4: Liberando todos los bloques...\n");
    for (int i = 0; i < NUM_OPERATIONS; i++) {
        if (blocks[i]) {
            free(blocks[i]);
        }
    }

    clock_t end_time = clock();
    size_t end_memory = get_memory_usage();

    double total_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    size_t memory_used = end_memory - start_memory;

    printf("RESULTADOS:\n");
    printf("  Tiempo total: %.4f segundos\n", total_time);
    printf("  Memoria utilizada: %zu KB\n", memory_used);
    printf("  Operaciones por segundo: %.0f\n", NUM_OPERATIONS * 3 / total_time);
}

int main() {
    printf("=== BENCHMARK COMPARATIVO: MEMORY MANAGER vs MALLOC ===\n");
    printf("Operaciones por prueba: %d\n", NUM_OPERATIONS);
    printf("Tamaño de bloques: %d - %d bytes\n\n", MIN_BLOCK_SIZE, MAX_BLOCK_SIZE);

    srand((unsigned int)time(NULL)); // ✅ CORRECCIÓN: Semilla adecuada

    benchmark_custom_memory_manager();
    benchmark_standard_malloc();

    return 0;
}

