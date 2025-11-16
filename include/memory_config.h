#ifndef MEMORY_CONFIG_H
#define MEMORY_CONFIG_H

#include <stddef.h>
#include <stdint.h>

// Configuración de alineación de memoria
#ifndef MEMORY_ALIGNMENT
#define MEMORY_ALIGNMENT 8
#endif

// Macros de alineación
#define ALIGN_SIZE(size) (((size) + (MEMORY_ALIGNMENT-1)) & ~(MEMORY_ALIGNMENT-1))
#define MIN_BLOCK_SIZE 32
#define MAGIC_NUMBER 0xDEADBEEF

// Estrategias de asignación
typedef enum {
    ALLOC_FIRST_FIT = 0,
    ALLOC_BEST_FIT = 1,
    ALLOC_WORST_FIT = 2,
    ALLOC_NEXT_FIT = 3
} alloc_strategy_t;

// Códigos de retorno estandarizados
typedef enum {
    MEMORY_SUCCESS = 0,
    MEMORY_ERROR_INVALID_PARAM = -1,
    MEMORY_ERROR_OUT_OF_MEMORY = -2,
    MEMORY_ERROR_CORRUPTION = -3,
    MEMORY_ERROR_CLIENT_INVALID = -4,
    MEMORY_ERROR_POOL_NOT_INIT = -5
} memory_status_t;

// Niveles de logging
typedef enum {
    MEMORY_LOG_DEBUG = 0,
    MEMORY_LOG_INFO = 1,
    MEMORY_LOG_WARN = 2,
    MEMORY_LOG_ERROR = 3
} memory_log_level_t;

// Feature flags
#ifdef MEMORY_DEBUG
#define MEMORY_LOG(level, ...) memory_log_internal(level, __FILE__, __LINE__, __VA_ARGS__)
#else
#define MEMORY_LOG(level, ...)
#endif

// API visibility
#ifdef _WIN32
    #ifdef MEMORY_EXPORTS
        #define MEMORY_API __declspec(dllexport)
    #else
        #define MEMORY_API __declspec(dllimport)
    #endif
#else
    #define MEMORY_API __attribute__((visibility("default")))
#endif

#endif // MEMORY_CONFIG_H
