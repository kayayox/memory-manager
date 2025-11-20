#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/memory_pool.h"
#include "../include/memory_client.h"
#include "../include/memory_metrics.h"

void test_size_accounting() {
    printf("=== AUDITORÍA DE TAMAÑOS Y OVERHEAD ===\n");

    memory_pool_t* pool = memory_pool_create(1024 * 1024, ALLOC_FIRST_FIT);
    memory_client_t* client = memory_client_create(1, pool);

    if (!pool || !client) {
        printf("Error al crear pool o cliente\n");
        return;
    }

    // Tomar métricas iniciales
    pool_metrics_t initial_metrics;
    memory_pool_get_metrics(pool, &initial_metrics);

    printf("Memoria inicial - Usada: %zu bytes, Libre: %zu bytes\n",
           initial_metrics.used_memory, initial_metrics.free_memory);

    // Alocar bloques de tamaños conocidos
    size_t test_sizes[] = {100, 200, 300, 400, 500};
    void* pointers[5];
    size_t total_requested = 0;

    printf("\n--- Análisis de Overhead por Asignación ---\n");

    for (int i = 0; i < 5; i++) {
        total_requested += test_sizes[i];
        pointers[i] = memory_client_alloc(client, test_sizes[i]);

        if (pointers[i]) {
            pool_metrics_t current_metrics;
            memory_pool_get_metrics(pool, &current_metrics);

            size_t memory_increase = current_metrics.used_memory - initial_metrics.used_memory;
            size_t expected_memory = total_requested;
            size_t overhead = memory_increase - expected_memory;
            double overhead_percent = (double)overhead / memory_increase * 100.0;

            printf("Solicitado: %4zu bytes | Acumulado: %4zu bytes | ",
                   test_sizes[i], total_requested);
            printf("Memoria real: %4zu bytes | Overhead: %3zu bytes (%5.1f%%)\n",
                   memory_increase, overhead, overhead_percent);
        } else {
            printf("Error asignando %zu bytes\n", test_sizes[i]);
        }
    }

    // Métricas finales
    pool_metrics_t final_metrics;
    memory_pool_get_metrics(pool, &final_metrics);

    printf("\n--- Resumen de Métricas ---\n");
    printf("Memoria total pool: %zu bytes\n", final_metrics.total_memory);
    printf("Memoria usada final: %zu bytes\n", final_metrics.used_memory);
    printf("Memoria libre final: %zu bytes\n", final_metrics.free_memory);
    printf("Bloques usados: %d\n", final_metrics.used_blocks);
    printf("Total solicitado: %zu bytes\n", total_requested);

    // Cálculo de overhead total
    size_t total_overhead = final_metrics.used_memory - total_requested;
    double total_overhead_percent = (double)total_overhead / final_metrics.used_memory * 100.0;

    printf("Overhead total: %zu bytes (%5.1f%%)\n", total_overhead, total_overhead_percent);
    printf("Overhead por bloque (aprox): %zu bytes\n", total_overhead / 5);

    // Prueba adicional: fragmentación
    printf("Fragmentación: %.1f%%\n", final_metrics.fragmentation);

    // Verificación de integridad
    if (memory_pool_check(pool)==0) {
        printf("    Integridad del pool verificada\n");
    } else {
        printf("    Problemas de integridad detectados\n");
    }

    // Limpieza
    memory_client_destroy(client);
    memory_pool_destroy(pool);

    printf("\n=== Auditoría completada ===\n");
}

void test_alignment_analysis() {
    printf("\n=== ANÁLISIS DE ALINEACIÓN ===\n");

    memory_pool_t* pool = memory_pool_create(1024 * 1024, ALLOC_FIRST_FIT);
    memory_client_t* client = memory_client_create(1, pool);

    if (!pool || !client) {
        printf("Error al crear pool o cliente\n");
        return;
    }

    // Probar diferentes tamaños para ver el efecto de la alineación
    size_t test_sizes[] = {1, 7, 8, 15, 16, 31, 32, 63, 64, 100};

    printf("Tamaños de prueba y su alineación:\n");
    printf("Size | Aligned | Difference\n");
    printf("-----|---------|-----------\n");

    for (int i = 0; i < 10; i++) {
        size_t aligned = ((test_sizes[i] + (8-1)) & ~(8-1));
        printf("%4zu | %7zu | %10zu\n", test_sizes[i], aligned, aligned - test_sizes[i]);
    }

    memory_client_destroy(client);
    memory_pool_destroy(pool);
}

int main() {
    test_size_accounting();
    test_alignment_analysis();
    return 0;
}
