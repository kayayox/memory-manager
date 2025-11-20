#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include "memory_config.h"

// Estructura opaca del pool
typedef struct memory_pool memory_pool_t;

// API principal del pool
MEMORY_API memory_pool_t* memory_pool_create(size_t total_size, alloc_strategy_t strategy);
MEMORY_API void memory_pool_destroy(memory_pool_t* pool);
MEMORY_API void* memory_pool_alloc(memory_pool_t* pool, size_t size, int client_id);
MEMORY_API int memory_pool_free(memory_pool_t* pool, void* ptr, int client_id);
MEMORY_API int memory_pool_set_strategy(memory_pool_t* pool, alloc_strategy_t strategy);
MEMORY_API alloc_strategy_t memory_pool_get_strategy(const memory_pool_t* pool);
MEMORY_API size_t memory_pool_get_total_size(const memory_pool_t* pool);
MEMORY_API int memory_pool_is_valid(const memory_pool_t* pool);

// Funciones de debug (solo disponibles en modo DEBUG)
#ifdef MEMORY_DEBUG
MEMORY_API void memory_pool_dump(const memory_pool_t* pool);
MEMORY_API void memory_pool_validate(const memory_pool_t* pool);
#endif

#endif // MEMORY_POOL_H
