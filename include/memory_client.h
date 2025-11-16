#ifndef MEMORY_CLIENT_H
#define MEMORY_CLIENT_H

#include "memory_config.h"
#include "memory_pool.h"

// Estructura opaca del cliente
typedef struct memory_client memory_client_t;

// API del cliente
MEMORY_API memory_client_t* memory_client_create(int id, memory_pool_t* pool);
MEMORY_API void memory_client_destroy(memory_client_t* client);
MEMORY_API void* memory_client_alloc(memory_client_t* client, size_t size);
MEMORY_API int memory_client_free(memory_client_t* client, void* ptr);
MEMORY_API void memory_client_free_all(memory_client_t* client);
MEMORY_API int memory_client_get_id(const memory_client_t* client);
MEMORY_API size_t memory_client_get_allocated_count(const memory_client_t* client);
MEMORY_API memory_pool_t* memory_client_get_pool(const memory_client_t* client);
MEMORY_API int memory_client_reassign_pool(memory_client_t* client, memory_pool_t* new_pool);

#endif // MEMORY_CLIENT_H
