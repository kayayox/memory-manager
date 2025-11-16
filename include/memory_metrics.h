#ifndef MEMORY_METRICS_H_INCLUDED
#define MEMORY_METRICS_H_INCLUDED



#endif // MEMORY_METRICS_H_INCLUDED
#ifndef MEMORY_METRICS_H
#define MEMORY_METRICS_H

#include "memory_config.h"

// Estructura para métricas del pool
typedef struct {
    size_t total_memory;
    size_t used_memory;
    size_t free_memory;
    double fragmentation;
    int block_count;
    int free_blocks;
    int used_blocks;
    size_t largest_free_block;
    size_t allocation_count;
    size_t free_count;
    size_t failed_allocations;
} pool_metrics_t;

// API de métricas
MEMORY_API void memory_pool_get_metrics(void* pool, pool_metrics_t* metrics);
MEMORY_API void memory_pool_print_metrics(void* pool);
MEMORY_API int memory_pool_check(void* pool);
MEMORY_API double memory_pool_get_fragmentation(void* pool);
MEMORY_API size_t memory_pool_get_used_memory(void* pool);
MEMORY_API size_t memory_pool_get_free_memory(void* pool);

#endif // MEMORY_METRICS_H
