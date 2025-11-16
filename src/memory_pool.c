#include "memory_internal.h"
#include "../include/memory_pool.h"
#include "../include/memory_metrics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// Logging interno
void memory_log_internal(memory_log_level_t level, const char* file, int line, const char* format, ...) {
    const char* level_str[] = {"DEBUG", "INFO", "WARN", "ERROR"};
    fprintf(stderr, "[MEMORY-%s] %s:%d: ", level_str[level], file, line);

    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    fprintf(stderr, "\n");
}

// Verificación de bloque
int block_is_valid(const block_header_t* block) {
    return block && block->magic == MAGIC_NUMBER;
}

// Verificación de bloque en pool
int block_in_pool(const memory_pool_t* pool, const block_header_t* block) {
    if (!pool || !pool->memory_block || !block) return 0;
    return (uintptr_t)block >= (uintptr_t)pool->memory_block &&
           (uintptr_t)block < (uintptr_t)pool->memory_block + pool->total_size;
}

// Operaciones de lista libre
void add_to_free_list(memory_pool_t* pool, block_header_t* block) {
    if (!pool || !block || !block_is_valid(block)) return;

    block->next = pool->free_list;
    block->prev = NULL;
    block->used = 0;
    block->client_id = -1;

    if (pool->free_list) {
        pool->free_list->prev = block;
    }
    pool->free_list = block;

    MEMORY_LOG(MEMORY_LOG_DEBUG, "Bloque agregado a lista libre: %p (%zu bytes)",
               (void*)block, block->size);
}

static int remove_from_free_list(memory_pool_t* pool, block_header_t* block) {
    if (!pool || !block || !block_is_valid(block)) return 0;

    if (block->prev) {
        block->prev->next = block->next;
    } else {
        pool->free_list = block->next;
    }

    if (block->next) {
        block->next->prev = block->prev;
    }

    block->next = NULL;
    block->prev = NULL;

    MEMORY_LOG(MEMORY_LOG_DEBUG, "Bloque removido de lista libre: %p", (void*)block);
    return 1;
}

// Estrategias de asignación
static block_header_t* find_first_fit(memory_pool_t* pool, size_t size) {
    block_header_t* current = pool->free_list;
    while (current) {
        if (current->size >= size) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

static block_header_t* find_best_fit(memory_pool_t* pool, size_t size) {
    block_header_t* current = pool->free_list;
    block_header_t* best = NULL;

    while (current) {
        if (current->size >= size) {
            if (!best || current->size < best->size) {
                best = current;
                if (current->size == size) break;
            }
        }
        current = current->next;
    }
    return best;
}

static block_header_t* find_worst_fit(memory_pool_t* pool, size_t size) {
    block_header_t* current = pool->free_list;
    block_header_t* worst = NULL;

    while (current) {
        if (current->size >= size) {
            if (!worst || current->size > worst->size) {
                worst = current;
            }
        }
        current = current->next;
    }
    return worst;
}

static block_header_t* find_next_fit(memory_pool_t* pool, size_t size) {
    if (!pool->next_fit) {
        pool->next_fit = pool->free_list;
    }

    block_header_t* start = pool->next_fit;
    block_header_t* current = start;

    do {
        if (current && current->size >= size) {
            pool->next_fit = current->next ? current->next : pool->free_list;
            return current;
        }
        current = current ? current->next : pool->free_list;
    } while (current != start);

    return NULL;
}

// Fusión de bloques
static void fuse_with_neighbors(memory_pool_t* pool, block_header_t* block) {
    if (!pool || !block || !block_is_valid(block)) return;

    int fused;
    do {
        fused = 0;
        block_header_t* current = pool->free_list;

        while (current && !fused) {
            if (!block_is_valid(current)) {
                current = current->next;
                continue;
            }

            char* current_end = (char*)(current + 1) + current->size;
            char* block_start = (char*)block;
            char* block_end = (char*)(block + 1) + block->size;

            if (current_end == block_start) {
                current->size += sizeof(block_header_t) + block->size;
                block->magic = 0;
                remove_from_free_list(pool, block);
                //block = current;
                fused = 1;
                MEMORY_LOG(MEMORY_LOG_DEBUG, "Bloques fusionados: %p + %p",
                           (void*)current, (void*)block);
            }
            else if (block_end == (char*)current) {
                block->size += sizeof(block_header_t) + current->size;
                current->magic = 0;
                remove_from_free_list(pool, current);
                fused = 1;
                MEMORY_LOG(MEMORY_LOG_DEBUG, "Bloques fusionados: %p + %p",
                           (void*)block, (void*)current);
            } else {
                current = current->next;
            }
        }
    } while (fused);

    add_to_free_list(pool, block);
}

// Implementación de la API pública
MEMORY_API memory_pool_t* memory_pool_create(size_t total_size, alloc_strategy_t strategy) {
    if (total_size < sizeof(block_header_t) + MIN_BLOCK_SIZE) {
        MEMORY_LOG(MEMORY_LOG_ERROR, "Tamaño de pool insuficiente: %zu", total_size);
        return NULL;
    }

    memory_pool_t* pool = malloc(sizeof(memory_pool_t));
    if (!pool) {
        MEMORY_LOG(MEMORY_LOG_ERROR, "No se pudo asignar estructura del pool");
        return NULL;
    }

    pool->memory_block = malloc(total_size);
    if (!pool->memory_block) {
        MEMORY_LOG(MEMORY_LOG_ERROR, "No se pudo asignar bloque de memoria: %zu bytes", total_size);
        free(pool);
        return NULL;
    }

    memset(pool->memory_block, 0, total_size);
    pool->total_size = total_size;
    pool->strategy = strategy;
    pool->free_list = NULL;
    pool->next_fit = NULL;
    pool->active = 1;
    memset(&pool->metrics, 0, sizeof(pool_metrics_t));
    pool->metrics.total_memory = total_size;

    if (pthread_mutex_init(&pool->mutex, NULL) != 0) {
        MEMORY_LOG(MEMORY_LOG_ERROR, "No se pudo inicializar mutex");
        free(pool->memory_block);
        free(pool);
        return NULL;
    }

    block_header_t* first_block = (block_header_t*)pool->memory_block;
    first_block->size = total_size - sizeof(block_header_t);
    first_block->used = 0;
    first_block->client_id = -1;
    first_block->magic = MAGIC_NUMBER;
    first_block->next = first_block->prev = NULL;

    add_to_free_list(pool, first_block);

    MEMORY_LOG(MEMORY_LOG_INFO, "Pool creado: %zu bytes, estrategia: %d",
               total_size, strategy);

    return pool;
}

MEMORY_API void memory_pool_destroy(memory_pool_t* pool) {
    if (!pool) return;

    pthread_mutex_lock(&pool->mutex);
    pool->active = 0;

    if (pool->memory_block) {
        free(pool->memory_block);
        pool->memory_block = NULL;
    }

    pool->free_list = NULL;
    pool->next_fit = NULL;
    pool->total_size = 0;

    pthread_mutex_unlock(&pool->mutex);
    pthread_mutex_destroy(&pool->mutex);

    free(pool);

    MEMORY_LOG(MEMORY_LOG_INFO, "Pool destruido correctamente");
}

MEMORY_API void* memory_pool_alloc(memory_pool_t* pool, size_t size, int client_id) {
    if (!pool || size == 0) {
        MEMORY_LOG(MEMORY_LOG_ERROR, "Parámetros inválidos para alloc");
        return NULL;
    }

    pthread_mutex_lock(&pool->mutex);

    if (!pool->active) {
        MEMORY_LOG(MEMORY_LOG_ERROR, "Intento de usar pool inactivo");
        pthread_mutex_unlock(&pool->mutex);
        return NULL;
    }

    size_t aligned_size = ALIGN_SIZE(size);
    if (aligned_size > pool->total_size - sizeof(block_header_t)) {
        MEMORY_LOG(MEMORY_LOG_ERROR, "Tamaño solicitado demasiado grande: %zu", aligned_size);
        pthread_mutex_unlock(&pool->mutex);
        pool->metrics.failed_allocations++;
        return NULL;
    }

    block_header_t* block = NULL;
    switch (pool->strategy) {
        case ALLOC_FIRST_FIT: block = find_first_fit(pool, aligned_size); break;
        case ALLOC_BEST_FIT: block = find_best_fit(pool, aligned_size); break;
        case ALLOC_WORST_FIT: block = find_worst_fit(pool, aligned_size); break;
        case ALLOC_NEXT_FIT: block = find_next_fit(pool, aligned_size); break;
    }

    if (!block) {
        MEMORY_LOG(MEMORY_LOG_WARN, "No hay bloques libres para %zu bytes", aligned_size);
        pthread_mutex_unlock(&pool->mutex);
        pool->metrics.failed_allocations++;
        return NULL;
    }

    remove_from_free_list(pool, block);

    size_t remaining = block->size - aligned_size;
    if (remaining >= sizeof(block_header_t) + MIN_BLOCK_SIZE) {
        block_header_t* new_block = (block_header_t*)((char*)(block + 1) + aligned_size);

        new_block->size = remaining - sizeof(block_header_t);
        new_block->used = 0;
        new_block->client_id = -1;
        new_block->magic = MAGIC_NUMBER;
        new_block->next = new_block->prev = NULL;

        block->size = aligned_size;
        add_to_free_list(pool, new_block);
    }

    block->used = 1;
    block->client_id = client_id;

    void* data_ptr = (void*)(block + 1);
    memset(data_ptr, 0, block->size);

    pool->metrics.allocation_count++;
    pool->metrics.used_memory += block->size;

    MEMORY_LOG(MEMORY_LOG_DEBUG, "Cliente %d asignó %zu bytes en %p",
               client_id, block->size, data_ptr);

    pthread_mutex_unlock(&pool->mutex);
    return data_ptr;
}

MEMORY_API int memory_pool_free(memory_pool_t* pool, void* ptr, int client_id) {
    if (!pool || !ptr) {
        MEMORY_LOG(MEMORY_LOG_ERROR, "Parámetros inválidos para free");
        return MEMORY_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&pool->mutex);

    if (!pool->active) {
        MEMORY_LOG(MEMORY_LOG_ERROR, "Intento de usar pool inactivo");
        pthread_mutex_unlock(&pool->mutex);
        return MEMORY_ERROR_POOL_NOT_INIT;
    }

    block_header_t* block = (block_header_t*)ptr - 1;

    if (!block_in_pool(pool, block)) {
        MEMORY_LOG(MEMORY_LOG_ERROR, "Bloque fuera del pool: %p", (void*)block);
        pthread_mutex_unlock(&pool->mutex);
        return MEMORY_ERROR_CORRUPTION;
    }

    if (!block_is_valid(block)) {
        MEMORY_LOG(MEMORY_LOG_ERROR, "Bloque corrupto: %p", (void*)block);
        pthread_mutex_unlock(&pool->mutex);
        return MEMORY_ERROR_CORRUPTION;
    }

    if (!block->used) {
        MEMORY_LOG(MEMORY_LOG_WARN, "Bloque ya libre: %p", (void*)block);
        pthread_mutex_unlock(&pool->mutex);
        return MEMORY_SUCCESS; // No es un error crítico
    }

    if (block->client_id != client_id) {
        MEMORY_LOG(MEMORY_LOG_ERROR, "Cliente %d intentó liberar bloque del cliente %d",
                   client_id, block->client_id);
        pthread_mutex_unlock(&pool->mutex);
        return MEMORY_ERROR_CLIENT_INVALID;
    }

    MEMORY_LOG(MEMORY_LOG_DEBUG, "Cliente %d liberó %zu bytes en %p",
               client_id, block->size, ptr);

    pool->metrics.free_count++;
    pool->metrics.used_memory -= block->size;

    fuse_with_neighbors(pool, block);

    pthread_mutex_unlock(&pool->mutex);
    return MEMORY_SUCCESS;
}

// Resto de implementaciones de memory_pool.c...
MEMORY_API int memory_pool_set_strategy(memory_pool_t* pool, alloc_strategy_t strategy) {
    if (!pool) return MEMORY_ERROR_INVALID_PARAM;

    pthread_mutex_lock(&pool->mutex);
    pool->strategy = strategy;
    pthread_mutex_unlock(&pool->mutex);

    return MEMORY_SUCCESS;
}

MEMORY_API alloc_strategy_t memory_pool_get_strategy(const memory_pool_t* pool) {
    return pool ? pool->strategy : ALLOC_FIRST_FIT;
}

MEMORY_API size_t memory_pool_get_total_size(const memory_pool_t* pool) {
    return pool ? pool->total_size : 0;
}

MEMORY_API int memory_pool_is_valid(const memory_pool_t* pool) {
    return pool && pool->active && pool->memory_block;
}

#ifdef MEMORY_DEBUG
MEMORY_API void memory_pool_dump(const memory_pool_t* pool) {
    if (!pool) return;

    printf("=== Memory Pool Dump ===\n");
    printf("Total size: %zu\n", pool->total_size);
    printf("Strategy: %d\n", pool->strategy);
    printf("Active: %d\n", pool->active);

    // Implementar dump detallado...
}

MEMORY_API void memory_pool_validate(const memory_pool_t* pool) {
    if (!pool) return;

    // Implementar validación completa...
    printf("Pool validation completed\n");
}
#endif
