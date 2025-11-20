#include "memory_internal.h"
#include "../include/memory_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// =============================================================================
// ESTRUCTURAS DE TABLA HASH
// =============================================================================

#define HASH_TABLE_INITIAL_CAPACITY 16
#define HASH_TABLE_LOAD_FACTOR_THRESHOLD 0.75
#define HASH_TABLE_GROWTH_FACTOR 2

typedef struct hash_node {
    void* ptr;
    struct hash_node* next;
} hash_node_t;

typedef struct {
    hash_node_t** buckets;
    size_t bucket_count;
    size_t element_count;
    double load_factor_threshold;
} hash_table_t;

// =============================================================================
// FUNCIONES INTERNAS DE TABLA HASH
// =============================================================================

static uint32_t hash_ptr(void* ptr) {
    uintptr_t key = (uintptr_t)ptr;

    // Mezcla de bits más efectiva
    key = (key ^ (key >> 30)) * UINT32_C(0xbf58476d1ce4e5b9);
    key = (key ^ (key >> 27)) * UINT32_C(0x94d049bb133111eb);
    key = key ^ (key >> 31);

    return (uint32_t)key;
}

static hash_table_t* hash_table_create(size_t initial_capacity) {
    hash_table_t* table = malloc(sizeof(hash_table_t));
    if (!table) return NULL;

    table->bucket_count = initial_capacity > 0 ? initial_capacity : HASH_TABLE_INITIAL_CAPACITY;
    table->element_count = 0;
    table->load_factor_threshold = HASH_TABLE_LOAD_FACTOR_THRESHOLD;
    table->buckets = calloc(table->bucket_count, sizeof(hash_node_t*));

    if (!table->buckets) {
        free(table);
        return NULL;
    }

    return table;
}

static void hash_table_destroy(hash_table_t* table) {
    if (!table) return;

    for (size_t i = 0; i < table->bucket_count; i++) {
        hash_node_t* current = table->buckets[i];
        while (current) {
            hash_node_t* next = current->next;
            free(current);
            current = next;
        }
    }

    free(table->buckets);
    free(table);
}

static int hash_table_resize(hash_table_t* table) {
    if (!table) return 0;

    size_t new_capacity = table->bucket_count * HASH_TABLE_GROWTH_FACTOR;
    hash_node_t** new_buckets = calloc(new_capacity, sizeof(hash_node_t*));
    if (!new_buckets) {
        MEMORY_LOG(MEMORY_LOG_ERROR, "No se pudo redimensionar tabla hash a %zu buckets", new_capacity);
        return 0;
    }

    // Rehash todos los elementos
    for (size_t i = 0; i < table->bucket_count; i++) {
        hash_node_t* current = table->buckets[i];
        while (current) {
            hash_node_t* next = current->next;
            uint32_t new_index = hash_ptr(current->ptr) % new_capacity;

            // Insertar en nueva tabla
            current->next = new_buckets[new_index];
            new_buckets[new_index] = current;

            current = next;
        }
    }

    free(table->buckets);
    table->buckets = new_buckets;
    table->bucket_count = new_capacity;

    MEMORY_LOG(MEMORY_LOG_DEBUG, "Tabla hash redimensionada a %zu buckets", new_capacity);
    return 1;
}

static int hash_table_insert(hash_table_t* table, void* ptr) {
    if (!table || !ptr) return 0;

    // Verificar factor de carga y redimensionar si es necesario
    double load_factor = (double)table->element_count / table->bucket_count;
    if (load_factor > HASH_TABLE_LOAD_FACTOR_THRESHOLD) {
        if (!hash_table_resize(table)) {
            MEMORY_LOG(MEMORY_LOG_WARN, "No se pudo redimensionar tabla hash, continuando con capacidad actual");
        }
    }

    uint32_t bucket_index = hash_ptr(ptr) % table->bucket_count;
    hash_node_t* new_node = malloc(sizeof(hash_node_t));
    if (!new_node) return 0;

    new_node->ptr = ptr;
    new_node->next = table->buckets[bucket_index];
    table->buckets[bucket_index] = new_node;
    table->element_count++;

    MEMORY_LOG(MEMORY_LOG_DEBUG, "Bloque %p insertado en bucket %u (elementos: %zu)",
               ptr, bucket_index, table->element_count);
    return 1;
}

static int hash_table_remove(hash_table_t* table, void* ptr) {
    if (!table || !ptr) return 0;

    uint32_t bucket_index = hash_ptr(ptr) % table->bucket_count;
    hash_node_t** current = &table->buckets[bucket_index];

    while (*current) {
        if ((*current)->ptr == ptr) {
            hash_node_t* to_remove = *current;
            *current = to_remove->next;
            free(to_remove);
            table->element_count--;
            return 1;
        }
        current = &(*current)->next;
    }

    return 0;
}

static void hash_table_clear(hash_table_t* table) {
    if (!table) return;

    for (size_t i = 0; i < table->bucket_count; i++) {
        hash_node_t* current = table->buckets[i];
        while (current) {
            hash_node_t* next = current->next;
            free(current);
            current = next;
        }
        table->buckets[i] = NULL;
    }
    table->element_count = 0;
}

// =============================================================================
// FUNCIÓN INTERNA PARA EVITAR DEADLOCK
// =============================================================================

static void memory_client_free_all_unsafe(memory_client_t* client) {
    if (!client) return;

    hash_table_t* table = (hash_table_t*)client->allocated_blocks;
    if (!table || table->element_count == 0) {
        return;
    }

    MEMORY_LOG(MEMORY_LOG_INFO, "Cliente %d liberando %zu bloques de tabla hash",
               client->id, table->element_count);

    for (size_t i = 0; i < table->bucket_count; i++) {
        hash_node_t* current = table->buckets[i];
        while (current) {
            if (current->ptr) {
                memory_pool_free(client->pool, current->ptr, client->id);
            }
            current = current->next;
        }
    }

    hash_table_clear(table);
}

// =============================================================================
// IMPLEMENTACIÓN CLIENTE CORREGIDA
// =============================================================================

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

    client->allocated_blocks = hash_table_create(HASH_TABLE_INITIAL_CAPACITY);
    if (!client->allocated_blocks) {
        MEMORY_LOG(MEMORY_LOG_ERROR, "No se pudo crear tabla hash del cliente");
        free(client);
        return NULL;
    }

    if (pthread_mutex_init(&client->mutex, NULL) != 0) {
        MEMORY_LOG(MEMORY_LOG_ERROR, "No se pudo inicializar mutex del cliente");
        hash_table_destroy((hash_table_t*)client->allocated_blocks);
        free(client);
        return NULL;
    }

    client->id = id;
    client->pool = pool;

    MEMORY_LOG(MEMORY_LOG_INFO, "Cliente %d creado con tabla hash", id);
    return client;
}

MEMORY_API void memory_client_destroy(memory_client_t* client) {
    if (!client) return;

    pthread_mutex_lock(&client->mutex);
    MEMORY_LOG(MEMORY_LOG_INFO, "Destruyendo cliente %d", client->id);

    memory_client_free_all_unsafe(client);

    if (client->allocated_blocks) {
        hash_table_destroy((hash_table_t*)client->allocated_blocks);
        client->allocated_blocks = NULL;
    }

    pthread_mutex_unlock(&client->mutex);
    pthread_mutex_destroy(&client->mutex);

    free(client);

    MEMORY_LOG(MEMORY_LOG_INFO, "Cliente %d destruido correctamente", client->id);
}

MEMORY_API void* memory_client_alloc(memory_client_t* client, size_t size) {
    if (!client) {
        MEMORY_LOG(MEMORY_LOG_ERROR, "Cliente inválido");
        return NULL;
    }

    void* block = memory_pool_alloc(client->pool, size, client->id);
    if (block) {
        pthread_mutex_lock(&client->mutex);
        if (!hash_table_insert((hash_table_t*)client->allocated_blocks, block)) {
            MEMORY_LOG(MEMORY_LOG_ERROR, "No se pudo insertar bloque en tabla hash");
            memory_pool_free(client->pool, block, client->id);
            pthread_mutex_unlock(&client->mutex);
            return NULL;
        }
        pthread_mutex_unlock(&client->mutex);
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
        pthread_mutex_lock(&client->mutex);
        if (hash_table_remove((hash_table_t*)client->allocated_blocks, ptr)) {
            MEMORY_LOG(MEMORY_LOG_DEBUG, "Cliente %d removió bloque %p de tabla hash",
                       client->id, ptr);
        } else {
            MEMORY_LOG(MEMORY_LOG_WARN, "Cliente %d intentó liberar bloque %p no registrado",
                       client->id, ptr);
        }
        pthread_mutex_unlock(&client->mutex);
    }
    return result;
}

MEMORY_API void memory_client_free_all(memory_client_t* client) {
    if (!client) {
        MEMORY_LOG(MEMORY_LOG_WARN, "Cliente inválido en free_all");
        return;
    }

    pthread_mutex_lock(&client->mutex);
    memory_client_free_all_unsafe(client);
    pthread_mutex_unlock(&client->mutex);

    MEMORY_LOG(MEMORY_LOG_INFO, "Cliente %d liberó todos los bloques", client->id);
}

MEMORY_API size_t memory_client_get_allocated_count(const memory_client_t* client) {
    if (!client || !client->allocated_blocks) return 0;

    pthread_mutex_lock((pthread_mutex_t*)&client->mutex);
    size_t count = ((hash_table_t*)client->allocated_blocks)->element_count;
    pthread_mutex_unlock((pthread_mutex_t*)&client->mutex);
    return count;
}

MEMORY_API int memory_client_get_id(const memory_client_t* client) {
    return client ? client->id : -1;
}

MEMORY_API memory_pool_t* memory_client_get_pool(const memory_client_t* client) {
    return client ? client->pool : NULL;
}

MEMORY_API int memory_client_reassign_pool(memory_client_t* client, memory_pool_t* new_pool) {
    if (!client || !new_pool) {
        return MEMORY_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&client->mutex);
    memory_client_free_all_unsafe(client);
    client->pool = new_pool;
    pthread_mutex_unlock(&client->mutex);

    MEMORY_LOG(MEMORY_LOG_INFO, "Cliente %d reasignado a nuevo pool", client->id);
    return MEMORY_SUCCESS;
}
