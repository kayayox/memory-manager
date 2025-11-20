#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/memory_pool.h"
#include "../include/memory_client.h"
#include "../include/memory_metrics.h"

memory_client_t* client;
memory_pool_t* pool;

typedef struct nodo {
    float dato;
    size_t id;
} nodo;

typedef struct lista {
    nodo A;
    struct lista* next;
} lista;

lista* crea_lista() {
    return NULL;
}

void insert(lista** A, float dato, size_t id) {
    lista* temp = memory_client_alloc(client, sizeof(lista));
    if (!temp) {
        printf("Error: No se pudo asignar memoria para el nodo\n");
        return;
    }
    temp->A.dato = dato;
    temp->A.id = id;
    temp->next = *A;
    *A = temp;
}

void liberar_lista(lista** A) {
    if (!A || !*A) return;
    *A = NULL;
}

int main() {
    pool = memory_pool_create(4024 * 1024, ALLOC_FIRST_FIT);
    if (!pool) {
        printf("Error al crear pool\n");
        return 1;
    }

    client = memory_client_create(1, pool);
    if (!client) {
        printf("Error al crear cliente\n");
        memory_pool_destroy(pool);
        return 1;
    }

    lista* pri = crea_lista();

    printf("Insertando 1024 elementos...\n");
    for (size_t i = 0; i < 1024; i++) {
        float x = i * 3.14156f;
        insert(&pri, x, i);

        // Verificar periódicamente el estado
        if (i % 100 == 0) {
            size_t allocated = memory_client_get_allocated_count(client);
            printf("Insertados %zu elementos, bloques asignados: %zu\n", i, allocated);
        }
    }

    printf("\n--- Métricas después de inserción ---\n");
    memory_pool_print_metrics(pool);

    // Verificar integridad antes de continuar
    if (memory_pool_check(pool)!=0) {
        printf("ERROR: Pool corrupto después de las inserciones\n");
        memory_client_destroy(client);
        memory_pool_destroy(pool);
        return 1;
    }

    printf("\n--- Recorriendo lista ---\n");
    lista* temp = pri;
    size_t count = 0;
    while (temp && count < 20) { // Limitar impresión a 20 elementos
        printf("%ld[%f]\n", temp->A.id, temp->A.dato);
        temp = temp->next;
        count++;
    }
    if (count >= 20) {
        printf("... (mostrando solo primeros 20 elementos)\n");
    }

    printf("\n--- Liberando memoria ---\n");
    liberar_lista(&pri);
    memory_client_free_all(client);

    printf("--- Métricas después de liberación ---\n");
    memory_pool_print_metrics(pool);

    // Verificar integridad final
    if (memory_pool_check(pool)!=0) {
        printf("ERROR: Pool corrupto después de la liberación\n");
    }

    memory_client_destroy(client);
    memory_pool_destroy(pool);

    printf("Programa completado exitosamente\n");
    return 0;
}
