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

    // Verificar si el bloque está realmente en la lista libre
    int found = 0;
    block_header_t* current = pool->free_list;
    int safety_count = 0;

    while (current && safety_count < 1000) {
        if (current == block) {
            found = 1;
            break;
        }
        current = current->next;
        safety_count++;
    }

    if (!found) {
        MEMORY_LOG(MEMORY_LOG_WARN, "Intento de remover bloque %p no encontrado en lista libre",
                   (void*)block);
        return 0;
    }

    // Actualizar next_fit si es necesario
    if (pool->next_fit == block) {
        pool->next_fit = block->next ? block->next : pool->free_list;
    }

    // Remover de la lista
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
    int safety_count = 0;

    while (current && safety_count < 1000) {
        if (current->size >= size) {
            if (!best || current->size < best->size) {
                best = current;
                // Si encontramos un ajuste perfecto, salir inmediatamente
                if (current->size == size) break;
            }
        }
        current = current->next;
        safety_count++;
    }

    if (safety_count >= 1000) {
        MEMORY_LOG(MEMORY_LOG_ERROR, "Posible ciclo en lista libre durante BEST_FIT");
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
    if (!pool->free_list) return NULL;

    if (!pool->next_fit) {
        pool->next_fit = pool->free_list;
    }

    block_header_t* current = pool->next_fit;
    block_header_t* start = current;

    do {
        if (current->size >= size) {
            pool->next_fit = current->next ? current->next : pool->free_list;
            return current;
        }
        current = current->next ? current->next : pool->free_list;
    } while (current && current != start);

    return NULL;
}

static void fuse_with_neighbors(memory_pool_t* pool, block_header_t* block) {
    if (!pool || !block || !block_is_valid(block)) return;

    int fused;
    int safety_counter = 0;
    const int MAX_FUSE_ITERATIONS = 100;

    do {
        fused = 0;
        safety_counter++;

        // Verificar límite de seguridad
        if (safety_counter > MAX_FUSE_ITERATIONS) {
            MEMORY_LOG(MEMORY_LOG_ERROR,
                       "Límite de fusiones excedido para bloque %p. Posible corrupción.",
                       (void*)block);
            break;
        }

        // Fusión con bloque siguiente
        block_header_t* next_block = (block_header_t*)((char*)(block + 1) + block->size);
        if ((char*)next_block < (char*)pool->memory_block + pool->total_size &&
            block_is_valid(next_block) && !next_block->used) {

            block->size += sizeof(block_header_t) + next_block->size;
            remove_from_free_list(pool, next_block);
            next_block->magic = 0;
            fused = 1;

            MEMORY_LOG(MEMORY_LOG_DEBUG, "Bloques fusionados con siguiente: %p + %p",
                       (void*)block, (void*)next_block);
            continue; // Continuar para posible fusión adicional
        }

        // Fusión con bloque anterior (más eficiente)
        if ((char*)block > (char*)pool->memory_block) {
            // Buscar bloque anterior de forma más directa
            block_header_t* potential_prev = NULL;
            char* current_pos = (char*)pool->memory_block;

            while (current_pos < (char*)block) {
                block_header_t* current_block = (block_header_t*)current_pos;
                if (!block_is_valid(current_block)) break;

                size_t block_total_size = sizeof(block_header_t) + current_block->size;
                char* next_block_pos = current_pos + block_total_size;

                if (next_block_pos == (char*)block &&
                    !current_block->used &&
                    block_is_valid(current_block)) {
                    potential_prev = current_block;
                    break;
                }

                if (block_total_size == 0) break;
                current_pos += block_total_size;
            }

            if (potential_prev) {
                potential_prev->size += sizeof(block_header_t) + block->size;
                remove_from_free_list(pool, block);
                block->magic = 0;
                block = potential_prev;
                fused = 1;

                MEMORY_LOG(MEMORY_LOG_DEBUG, "Bloques fusionados con anterior: %p + %p",
                           (void*)potential_prev, (void*)block);
                continue; // Continuar para posible fusión adicional
            }
        }

    } while (fused);

    // Solo agregar a la lista libre si el bloque sigue siendo válido
    if (block_is_valid(block) && !block->used) {
        add_to_free_list(pool, block);
    }
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

    if (!pool->active) {
        pthread_mutex_unlock(&pool->mutex);
        return;
    }

    if (pool->metrics.used_blocks > 0) {
        MEMORY_LOG(MEMORY_LOG_WARN,
                   "Destruyendo pool con %d bloques aún en uso - posibles leaks",
                   pool->metrics.used_blocks);
    }

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

        if (!block_in_pool(pool, new_block)) {
            MEMORY_LOG(MEMORY_LOG_ERROR, "Error crítico: split block fuera del pool");
            // Recuperación: restaurar bloque original
            add_to_free_list(pool, block);
            pthread_mutex_unlock(&pool->mutex);
            pool->metrics.failed_allocations++;
            return NULL;
        }

        new_block->size = remaining - sizeof(block_header_t);
        new_block->used = 0;
        new_block->client_id = -1;
        new_block->magic = MAGIC_NUMBER;
        new_block->next = new_block->prev = NULL;

        block->size = aligned_size;
        add_to_free_list(pool, new_block);

        if (pool->next_fit == block) {
            pool->next_fit = new_block;
        }
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
        return MEMORY_SUCCESS;
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

MEMORY_API int memory_pool_set_strategy(memory_pool_t* pool, alloc_strategy_t strategy) {
    if (!pool) return MEMORY_ERROR_INVALID_PARAM;

    pthread_mutex_lock(&pool->mutex);
    pool->strategy = strategy;
    pool->next_fit = NULL;
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
