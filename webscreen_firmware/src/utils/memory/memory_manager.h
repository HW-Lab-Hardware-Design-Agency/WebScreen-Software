/**
 * @file memory_manager.h
 * @brief Unified memory management system for WebScreen
 * 
 * Provides a unified interface for memory allocation with fallback strategies
 * and memory usage tracking for debugging and optimization.
 */

#pragma once

#include <Arduino.h>
#include <esp_heap_caps.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Memory allocation strategies
 */
typedef enum {
    MEMORY_STRATEGY_INTERNAL_ONLY,  ///< Use only internal RAM
    MEMORY_STRATEGY_PSRAM_PREFERRED, ///< Prefer PSRAM, fallback to internal
    MEMORY_STRATEGY_PSRAM_ONLY,     ///< Use only PSRAM (may fail)
    MEMORY_STRATEGY_AUTO            ///< Automatically choose best strategy
} memory_strategy_t;

/**
 * @brief Memory allocation statistics
 */
typedef struct {
    size_t total_allocated;         ///< Total bytes allocated
    size_t peak_allocated;          ///< Peak allocation in bytes
    size_t internal_free;           ///< Free internal RAM
    size_t psram_free;              ///< Free PSRAM
    uint32_t allocation_count;      ///< Number of active allocations
    uint32_t failed_allocations;    ///< Number of failed allocations
} memory_stats_t;

/**
 * @brief Initialize the memory management system
 * @return true if initialization successful, false otherwise
 */
bool memory_manager_init(void);

/**
 * @brief Allocate memory using the specified strategy
 * @param size Size in bytes to allocate
 * @param strategy Memory allocation strategy to use
 * @param caller Name of calling function (for debugging)
 * @return Pointer to allocated memory or NULL on failure
 */
void* memory_alloc(size_t size, memory_strategy_t strategy, const char* caller);

/**
 * @brief Free previously allocated memory
 * @param ptr Pointer to memory to free
 * @param caller Name of calling function (for debugging)
 */
void memory_free(void* ptr, const char* caller);

/**
 * @brief Reallocate memory with new size
 * @param ptr Existing memory pointer
 * @param new_size New size in bytes
 * @param strategy Memory allocation strategy to use
 * @param caller Name of calling function (for debugging)
 * @return Pointer to reallocated memory or NULL on failure
 */
void* memory_realloc(void* ptr, size_t new_size, memory_strategy_t strategy, const char* caller);

/**
 * @brief Get current memory statistics
 * @param stats Pointer to statistics structure to fill
 */
void memory_get_stats(memory_stats_t* stats);

/**
 * @brief Print detailed memory report to serial
 */
void memory_print_report(void);

/**
 * @brief Check if PSRAM is available and working
 * @return true if PSRAM is available, false otherwise
 */
bool memory_psram_available(void);

/**
 * @brief Get recommended strategy based on current memory state
 * @param required_size Size of allocation being planned
 * @return Recommended memory strategy
 */
memory_strategy_t memory_get_recommended_strategy(size_t required_size);

// Convenience macros for common allocations
#define MEMORY_ALLOC(size) memory_alloc(size, MEMORY_STRATEGY_AUTO, __FUNCTION__)
#define MEMORY_ALLOC_INTERNAL(size) memory_alloc(size, MEMORY_STRATEGY_INTERNAL_ONLY, __FUNCTION__)
#define MEMORY_ALLOC_PSRAM(size) memory_alloc(size, MEMORY_STRATEGY_PSRAM_PREFERRED, __FUNCTION__)
#define MEMORY_FREE(ptr) memory_free(ptr, __FUNCTION__)
#define MEMORY_REALLOC(ptr, size) memory_realloc(ptr, size, MEMORY_STRATEGY_AUTO, __FUNCTION__)

#ifdef __cplusplus
}
#endif