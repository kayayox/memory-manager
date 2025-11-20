#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include "../include/memory_pool.h"
#include "../include/memory_client.h"

#define NUM_THREADS 4
#define OPS_PER_THREAD 1000
#define MAX_BLOCK_SIZE 512

typedef struct {
    memory_pool_t* pool;
    int thread_id;
    int operations;
    double time_taken;
    unsigned int seed;
} thread_data_t;

void* thread_work_custom(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    clock_t start = clock();

    data->seed = (unsigned int)(time(NULL) ^ data->thread_id ^ (uintptr_t)pthread_self());

    memory_client_t* client = memory_client_create(data->thread_id, data->pool);
    if (!client) {
        printf("Error: No se pudo crear cliente para hilo %d\n", data->thread_id);
        data->time_taken = 0.0;
        return NULL;
    }

    void* blocks[OPS_PER_THREAD] = {0};

    for (int i = 0; i < data->operations; i++) {
        size_t size = 16 + (rand_r(&data->seed) % MAX_BLOCK_SIZE);
        blocks[i] = memory_client_alloc(client, size);

        if (blocks[i]) {
            // Uso del bloque
            memset(blocks[i], data->thread_id, size);

            // Liberar aproximadamente el 30% de los bloques inmediatamente
            if (rand_r(&data->seed) % 100 < 30) {
                memory_client_free(client, blocks[i]);
                blocks[i] = NULL;
            }
        }
    }

    // Limpiar bloques restantes
    for (int i = 0; i < data->operations; i++) {
        if (blocks[i]) {
            memory_client_free(client, blocks[i]);
        }
    }

    memory_client_destroy(client);

    clock_t end = clock();
    data->time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;

    return NULL;
}

void* thread_work_standard(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    clock_t start = clock();

    data->seed = (unsigned int)(time(NULL) ^ data->thread_id ^ (uintptr_t)pthread_self());

    void* blocks[OPS_PER_THREAD] = {0};

    for (int i = 0; i < data->operations; i++) {
        size_t size = 16 + (rand_r(&data->seed) % MAX_BLOCK_SIZE);
        blocks[i] = malloc(size);

        if (blocks[i]) {
            // Uso del bloque
            memset(blocks[i], data->thread_id, size);

            // Liberar aproximadamente el 30% de los bloques inmediatamente
            if (rand_r(&data->seed) % 100 < 30) {
                free(blocks[i]);
                blocks[i] = NULL;
            }
        }
    }

    // Limpiar bloques restantes
    for (int i = 0; i < data->operations; i++) {
        if (blocks[i]) {
            free(blocks[i]);
        }
    }

    clock_t end = clock();
    data->time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;

    return NULL;
}

void benchmark_concurrent_custom() {
    printf("=== CONCURRENT BENCHMARK: MEMORY MANAGER ===\n");

    memory_pool_t* pool = memory_pool_create(10 * 1024 * 1024, ALLOC_FIRST_FIT);
    if (!pool) {
        printf("Error: No se pudo crear pool\n");
        return;
    }

    pthread_t threads[NUM_THREADS];
    thread_data_t thread_data[NUM_THREADS];

    clock_t total_start = clock();

    // Crear hilos
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].pool = pool;
        thread_data[i].thread_id = i + 1;
        thread_data[i].operations = OPS_PER_THREAD;
        thread_data[i].time_taken = 0;

        if (pthread_create(&threads[i], NULL, thread_work_custom, &thread_data[i]) != 0) {
            printf("Error creando hilo %d\n", i);
            thread_data[i].time_taken = 0;
        }
    }

    // Esperar a que terminen todos los hilos
    double max_time = 0;
    double total_ops = 0;

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
        if (thread_data[i].time_taken > max_time) {
            max_time = thread_data[i].time_taken;
        }
        total_ops += thread_data[i].operations;
    }

    clock_t total_end = clock();
    double total_time = ((double)(total_end - total_start)) / CLOCKS_PER_SEC;

    printf("Hilos: %d, Operaciones por hilo: %d\n", NUM_THREADS, OPS_PER_THREAD);
    printf("Tiempo total: %.4f segundos\n", total_time);
    printf("Tiempo del hilo más lento: %.4f segundos\n", max_time);
    printf("Operaciones totales: %.0f\n", total_ops);
    printf("Operaciones por segundo: %.0f\n", total_ops / total_time);

    memory_pool_destroy(pool);
}

void benchmark_concurrent_standard() {
    printf("\n=== CONCURRENT BENCHMARK: MALLOC/FREE ===\n");

    pthread_t threads[NUM_THREADS];
    thread_data_t thread_data[NUM_THREADS];

    clock_t total_start = clock();

    // Crear hilos
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].pool = NULL;
        thread_data[i].thread_id = i + 1;
        thread_data[i].operations = OPS_PER_THREAD;
        thread_data[i].time_taken = 0;

        if (pthread_create(&threads[i], NULL, thread_work_standard, &thread_data[i]) != 0) {
            printf("Error creando hilo %d\n", i);
            thread_data[i].time_taken = 0;
        }
    }

    // Esperar a que terminen todos los hilos
    double max_time = 0;
    double total_ops = 0;

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
        if (thread_data[i].time_taken > max_time) {
            max_time = thread_data[i].time_taken;
        }
        total_ops += thread_data[i].operations;
    }

    clock_t total_end = clock();
    double total_time = ((double)(total_end - total_start)) / CLOCKS_PER_SEC;

    printf("Hilos: %d, Operaciones por hilo: %d\n", NUM_THREADS, OPS_PER_THREAD);
    printf("Tiempo total: %.4f segundos\n", total_time);
    printf("Tiempo del hilo más lento: %.4f segundos\n", max_time);
    printf("Operaciones totales: %.0f\n", total_ops);
    printf("Operaciones por segundo: %.0f\n", total_ops / total_time);
}

int main() {
    printf("=== BENCHMARK CONCURRENTE COMPARATIVO ===\n");

    srand((unsigned int)time(NULL));

    benchmark_concurrent_custom();
    benchmark_concurrent_standard();

    return 0;
}
