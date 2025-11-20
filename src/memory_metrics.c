#include "memory_internal.h"
#include "../include/memory_metrics.h"
#include <stdio.h>
#include <string.h>

MEMORY_API void memory_pool_get_metrics(void* pool_ptr, pool_metrics_t* metrics) {
    memory_pool_t* pool = (memory_pool_t*)pool_ptr;
    if (!pool || !metrics) return;

    pthread_mutex_lock(&pool->mutex);

    memset(metrics, 0, sizeof(pool_metrics_t));
    metrics->total_memory = pool->total_size;

    char* current = (char*)pool->memory_block;
    char* end = current + pool->total_size;

    while (current < end) {
        block_header_t* block = (block_header_t*)current;

        if (!block_is_valid(block)) break;

        size_t block_total_size = sizeof(block_header_t) + block->size;

        metrics->block_count++;
        if (block->used) {
            metrics->used_memory += block_total_size;
            metrics->used_blocks++;
        } else {
            metrics->free_memory += block_total_size;
            metrics->free_blocks++;
            if (block_total_size > metrics->largest_free_block) {
                metrics->largest_free_block = block_total_size;
            }
        }

        if (block_total_size == 0) break;
        current += block_total_size;
    }
    if (metrics->free_blocks > 1 && metrics->free_memory > 0) {
        double fragmentation = (1.0 - ((double)metrics->largest_free_block / metrics->free_memory)) * 100.0;
        metrics->fragmentation = fragmentation > 0.0 ? fragmentation : 0.0;
    } else {
        metrics->fragmentation = 0.0;
    }

    metrics->allocation_count = pool->metrics.allocation_count;
    metrics->free_count = pool->metrics.free_count;
    metrics->failed_allocations = pool->metrics.failed_allocations;

    pthread_mutex_unlock(&pool->mutex);
}

MEMORY_API void memory_pool_print_metrics(void* pool_ptr) {
    pool_metrics_t metrics;
    memory_pool_get_metrics(pool_ptr, &metrics);

    printf("\n=== MÉTRICAS DEL POOL ===\n");
    printf("Memoria total: %zu bytes\n", metrics.total_memory);
    printf("Memoria usada: %zu bytes (%.1f%%)\n",
           metrics.used_memory,
           (double)metrics.used_memory / metrics.total_memory * 100);
    printf("Memoria libre: %zu bytes (%.1f%%)\n",
           metrics.free_memory,
           (double)metrics.free_memory / metrics.total_memory * 100);
    printf("Bloques totales: %d\n", metrics.block_count);
    printf("Bloques usados: %d\n", metrics.used_blocks);
    printf("Bloques libres: %d\n", metrics.free_blocks);
    printf("Mayor bloque libre: %zu bytes\n", metrics.largest_free_block);
    printf("Fragmentación: %.1f%%\n", metrics.fragmentation);
    printf("Asignaciones: %zu\n", metrics.allocation_count);
    printf("Liberaciones: %zu\n", metrics.free_count);
    printf("Asignaciones fallidas: %zu\n", metrics.failed_allocations);
}

MEMORY_API int memory_pool_check(void* pool_ptr) {
    memory_pool_t* pool = (memory_pool_t*)pool_ptr;
    if (!pool) return 0;

    pthread_mutex_lock(&pool->mutex);

    int errors = 0;
    block_header_t* current = pool->free_list;
    int iteration = 0;

    while (current && iteration < 1000) {
        if (!block_is_valid(current)) {
            MEMORY_LOG(MEMORY_LOG_ERROR, "Bloque inválido en free_list: %p", (void*)current);
            errors++;
            break;
        }

        if (current->used) {
            MEMORY_LOG(MEMORY_LOG_ERROR, "Bloque marcado como usado en free_list: %p", (void*)current);
            errors++;
        }

        if (!block_in_pool(pool, current)) {
            MEMORY_LOG(MEMORY_LOG_ERROR, "Bloque fuera del pool en free_list: %p", (void*)current);
            errors++;
            break;
        }

        current = current->next;
        iteration++;
    }

    if (iteration >= 1000) {
        MEMORY_LOG(MEMORY_LOG_ERROR, "Posible ciclo en free_list");
        errors++;
    }

    pthread_mutex_unlock(&pool->mutex);
    return errors == 0;
}

MEMORY_API double memory_pool_get_fragmentation(void* pool_ptr) {
    pool_metrics_t metrics;
    memory_pool_get_metrics(pool_ptr, &metrics);
    return metrics.fragmentation;
}

MEMORY_API size_t memory_pool_get_used_memory(void* pool_ptr) {
    pool_metrics_t metrics;
    memory_pool_get_metrics(pool_ptr, &metrics);
    return metrics.used_memory;
}

MEMORY_API size_t memory_pool_get_free_memory(void* pool_ptr) {
    pool_metrics_t metrics;
    memory_pool_get_metrics(pool_ptr, &metrics);
    return metrics.free_memory;
}
