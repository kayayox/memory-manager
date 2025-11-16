#include "memory_internal.h"
#include "../include/memory_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

MEMORY_API memory_client_t* memory_client_create(int id, memory_pool_t* pool) {
    if (!pool || id < 0) {
        MEMORY_LOG(MEMORY_LOG_ERROR, "Parámetros inválidos para crear cliente");
        return NULL;
    }

    memory_client_t* client = malloc(sizeof(memory_client_t));
    if (!client) {
        MEMORY_LOG(MEMORY_LOG_ERROR, "No se pudo asignar estructura del cliente");
        return NULL;
    }

    client->id = id;
    client->pool = pool;
    client->block_capacity = 10;
    client->block_count = 0;
    client->allocated_blocks = malloc(sizeof(void*) * client->block_capacity);

    if (!client->allocated_blocks) {
        MEMORY_LOG(MEMORY_LOG_ERROR, "No se pudo asignar array de bloques del cliente");
        free(client);
        return NULL;
    }

    MEMORY_LOG(MEMORY_LOG_INFO, "Cliente %d creado", id);
    return client;
}

MEMORY_API void memory_client_destroy(memory_client_t* client) {
    if (!client) return;

    int client_id = client->id;
    MEMORY_LOG(MEMORY_LOG_INFO, "Destruyendo cliente %d", client_id);

    memory_client_free_all(client);

    if (client->allocated_blocks) {
        free(client->allocated_blocks);
    }

    free(client);

    MEMORY_LOG(MEMORY_LOG_INFO, "Cliente %d destruido correctamente", client_id);
}

MEMORY_API void* memory_client_alloc(memory_client_t* client, size_t size) {
    if (!client) {
        MEMORY_LOG(MEMORY_LOG_ERROR, "Cliente inválido");
        return NULL;
    }

    void* block = memory_pool_alloc(client->pool, size, client->id);
    if (block) {
        if (client->block_count >= client->block_capacity) {
            size_t new_capacity = client->block_capacity * 2;
            void** new_blocks = realloc(client->allocated_blocks, sizeof(void*) * new_capacity);
            if (!new_blocks) {
                MEMORY_LOG(MEMORY_LOG_ERROR, "No se pudo redimensionar array de bloques del cliente");
                memory_pool_free(client->pool, block, client->id);
                return NULL;
            }
            client->allocated_blocks = new_blocks;
            client->block_capacity = new_capacity;
        }
        client->allocated_blocks[client->block_count++] = block;

        MEMORY_LOG(MEMORY_LOG_DEBUG, "Cliente %d registró bloque %p", client->id, block);
    }

    return block;
}

MEMORY_API int memory_client_free(memory_client_t* client, void* ptr) {
    if (!client || !ptr) {
        MEMORY_LOG(MEMORY_LOG_ERROR, "Parámetros inválidos para client_free");
        return MEMORY_ERROR_INVALID_PARAM;
    }

    int result = memory_pool_free(client->pool, ptr, client->id);
    if (result == MEMORY_SUCCESS) {
        for (size_t i = 0; i < client->block_count; i++) {
            if (client->allocated_blocks[i] == ptr) {
                client->allocated_blocks[i] = client->allocated_blocks[client->block_count - 1];
                client->block_count--;
                MEMORY_LOG(MEMORY_LOG_DEBUG, "Cliente %d removió bloque %p del registro",
                           client->id, ptr);
                break;
            }
        }
    }

    return result;
}

MEMORY_API void memory_client_free_all(memory_client_t* client) {
    if (!client || !client->allocated_blocks || client->block_count == 0) {
        return;
    }

    MEMORY_LOG(MEMORY_LOG_INFO, "Cliente %d liberando %zu bloques",
               client->id, client->block_count);

    for (size_t i = 0; i < client->block_count; i++) {
        if (client->allocated_blocks[i]) {
            if (client->pool && memory_pool_is_valid(client->pool)) {
                block_header_t* block = (block_header_t*)client->allocated_blocks[i] - 1;

                uintptr_t block_addr = (uintptr_t)block;
                uintptr_t pool_start = (uintptr_t)client->pool->memory_block;
                uintptr_t pool_end = pool_start + client->pool->total_size;

                if (block_addr >= pool_start && block_addr < pool_end) {
                    if (block_is_valid(block) && block->used && block->client_id == client->id) {
                        MEMORY_LOG(MEMORY_LOG_DEBUG, "Liberando bloque %zu en %p",
                                   i, client->allocated_blocks[i]);
                        memory_pool_free(client->pool, client->allocated_blocks[i], client->id);
                    }
                }
            }
            client->allocated_blocks[i] = NULL;
        }
    }
    client->block_count = 0;

    MEMORY_LOG(MEMORY_LOG_INFO, "Cliente %d liberó todos los bloques", client->id);
}

MEMORY_API int memory_client_get_id(const memory_client_t* client) {
    return client ? client->id : -1;
}

MEMORY_API size_t memory_client_get_allocated_count(const memory_client_t* client) {
    return client ? client->block_count : 0;
}

MEMORY_API memory_pool_t* memory_client_get_pool(const memory_client_t* client) {
    return client ? client->pool : NULL;
}

MEMORY_API int memory_client_reassign_pool(memory_client_t* client, memory_pool_t* new_pool) {
    if (!client || !new_pool) {
        return MEMORY_ERROR_INVALID_PARAM;
    }

    memory_client_free_all(client);
    client->pool = new_pool;

    MEMORY_LOG(MEMORY_LOG_INFO, "Cliente %d reasignado a nuevo pool", client->id);
    return MEMORY_SUCCESS;
}
