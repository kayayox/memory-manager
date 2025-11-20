#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/memory_pool.h"
#include "../include/memory_client.h"
#include "../include/memory_metrics.h"

#define NUM_OPERATIONS 300  // Más reducido para estabilidad
#define NUM_ITERATIONS 2

typedef struct {
    const char* name;
    alloc_strategy_t strategy;
    double total_time;
    size_t memory_used;
    double fragmentation;
    int successful_ops;
} strategy_result_t;

void benchmark_strategy(alloc_strategy_t strategy, const char* name, strategy_result_t* result) {
    printf("Probando estrategia: %s\n", name);

    clock_t total_time = 0;
    size_t total_memory = 0;
    double total_fragmentation = 0;
    int total_successful = 0;

    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        printf("  Iteración %d...\n", iter + 1);

        clock_t start_time = clock();

        memory_pool_t* pool = memory_pool_create(2 * 1024 * 1024, strategy);
        memory_client_t* client = memory_client_create(1, pool);

        if (!pool || !client) {
            printf("    Error creando pool/cliente\n");
            continue;
        }

        void* blocks[NUM_OPERATIONS] = {0};
        int successful = 0;

        // Fase 1: Asignaciones simples
        for (int i = 0; i < NUM_OPERATIONS; i++) {
            size_t size = 64 + (rand() % 256); // Tamaños más uniformes
            blocks[i] = memory_client_alloc(client, size);

            if (blocks[i]) {
                successful++;
                // Inicialización mínima para prueba
                if (i % 20 == 0) {
                    memset(blocks[i], 0xAA, size);
                }
            }

            // Verificación periódica más frecuente
            if (i % 50 == 0 && i > 0) {
                if (!memory_pool_check(pool)) {
                    printf("    ERROR: Pool corrupto en operación %d - abortando iteración\n", i);
                    // Limpiar y salir de esta iteración
                    for (int j = 0; j < i; j++) {
                        if (blocks[j]) {
                            memory_client_free(client, blocks[j]);
                        }
                    }
                    successful = 0; // Marcar como fallida
                    break;
                }
            }
        }

        printf("    Asignaciones exitosas: %d/%d\n", successful, NUM_OPERATIONS);

        // Solo continuar si no hubo corrupción
        if (successful > 0 && memory_pool_check(pool)) {
            // Fase 2: Liberación estratégica (patrón simple)
            int freed = 0;
            for (int i = 0; i < NUM_OPERATIONS; i += 3) { // Liberar cada tercer bloque
                if (blocks[i]) {
                    if (memory_client_free(client, blocks[i]) == MEMORY_SUCCESS) {
                        blocks[i] = NULL;
                        freed++;
                    }
                }
            }

            // Fase 3: Re-asignaciones simples
            int reallocated = 0;
            for (int i = 0; i < NUM_OPERATIONS && reallocated < freed; i++) {
                if (!blocks[i]) {
                    size_t size = 64 + (rand() % 256);
                    blocks[i] = memory_client_alloc(client, size);
                    if (blocks[i]) {
                        reallocated++;
                    }
                }
            }

            // Obtener métricas finales
            pool_metrics_t metrics;
            memory_pool_get_metrics(pool, &metrics);

            total_fragmentation += metrics.fragmentation;
            total_memory += metrics.used_memory;
        }

        total_successful += successful;

        // Limpiar todos los bloques restantes
        for (int i = 0; i < NUM_OPERATIONS; i++) {
            if (blocks[i]) {
                memory_client_free(client, blocks[i]);
            }
        }

        memory_client_destroy(client);
        memory_pool_destroy(pool);

        clock_t end_time = clock();
        double iter_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
        total_time += iter_time;

        printf("    Tiempo: %.3fs, Éxito: %d/%d\n", iter_time, successful, NUM_OPERATIONS);
    }

    result->name = name;
    result->strategy = strategy;
    result->total_time = total_time / NUM_ITERATIONS;
    result->memory_used = total_memory / NUM_ITERATIONS;
    result->fragmentation = total_fragmentation / NUM_ITERATIONS;
    result->successful_ops = total_successful / NUM_ITERATIONS;
}

int main() {
    printf("=== BENCHMARK ESTRATEGIAS DE ASIGNACIÓN (CORREGIDO) ===\n");
    printf("Iteraciones por estrategia: %d\n", NUM_ITERATIONS);
    printf("Operaciones por iteración: %d\n\n", NUM_OPERATIONS);

    srand((unsigned int)time(NULL));

    strategy_result_t strategies[] = {
        {"FIRST_FIT", ALLOC_FIRST_FIT, 0, 0, 0, 0},
        {"BEST_FIT", ALLOC_BEST_FIT, 0, 0, 0, 0},
        {"WORST_FIT", ALLOC_WORST_FIT, 0, 0, 0, 0},
        {"NEXT_FIT", ALLOC_NEXT_FIT, 0, 0, 0, 0}
    };

    int num_strategies = sizeof(strategies) / sizeof(strategies[0]);

    for (int i = 0; i < num_strategies; i++) {
        benchmark_strategy(strategies[i].strategy, strategies[i].name, &strategies[i]);
    }

    // Mostrar resultados
    printf("\n=== RESULTADOS ===\n");
    printf("%-12s %-12s %-12s %-12s %-12s\n",
           "Estrategia", "Tiempo(s)", "Memoria(B)", "Fragmentación(%)", "Éxito(%)");
    printf("------------ ------------ ------------ ------------ ------------\n");

    for (int i = 0; i < num_strategies; i++) {
        double success_rate = ((double)strategies[i].successful_ops / (NUM_OPERATIONS * NUM_ITERATIONS)) * 100.0;
        printf("%-12s %-12.4f %-12zu %-12.2f %-12.1f\n",
               strategies[i].name,
               strategies[i].total_time,
               strategies[i].memory_used,
               strategies[i].fragmentation,
               success_rate);
    }

    printf("\nBenchmark completado.\n");
    return 0;
}
