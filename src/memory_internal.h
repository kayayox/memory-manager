#ifndef MEMORY_INTERNAL_H
#define MEMORY_INTERNAL_H

#include "../include/memory_config.h"
#include "../include/memory_pool.h"
#include "../include/memory_client.h"
#include "../include/memory_metrics.h"

#include <pthread.h>

// Estructura del header de bloque (interna)
typedef struct block_header {
    size_t size;
    struct block_header* next;
    struct block_header* prev;
    uint8_t used;
    uint32_t magic;
    int client_id;
} block_header_t;

// Estructura completa del pool (interna)
struct memory_pool {
    void* memory_block;
    size_t total_size;
    block_header_t* free_list;
    alloc_strategy_t strategy;
    block_header_t* next_fit;
    pthread_mutex_t mutex;
    pool_metrics_t metrics;
    int active;
};

// Estructura completa del cliente (interna)
struct memory_client {
    int id;
    memory_pool_t* pool;
    void* allocated_blocks;
    pthread_mutex_t mutex;
};

// Funciones internas (no exportadas)
extern int block_is_valid(const block_header_t* block);
extern int block_in_pool(const memory_pool_t* pool, const block_header_t* block);
extern void add_to_free_list(memory_pool_t* pool, block_header_t* block);
extern void memory_log_internal(memory_log_level_t level, const char* file, int line, const char* format, ...);

#endif // MEMORY_INTERNAL_H
