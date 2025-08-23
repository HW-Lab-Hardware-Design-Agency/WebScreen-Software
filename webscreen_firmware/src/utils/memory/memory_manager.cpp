/**
 * @file memory_manager.cpp
 * @brief Implementation of unified memory management system
 */

#include "memory_manager.h"
#include "../logging/logger.h"
#include <esp_heap_caps.h>

// Internal state
static memory_stats_t g_memory_stats = {0};
static bool g_memory_manager_initialized = false;
static bool g_psram_available = false;

// Memory allocation tracking (simple linked list for debugging)
typedef struct alloc_entry {
    void* ptr;
    size_t size;
    const char* caller;
    uint32_t timestamp;
    struct alloc_entry* next;
} alloc_entry_t;

static alloc_entry_t* g_alloc_list = nullptr;

bool memory_manager_init(void) {
    if (g_memory_manager_initialized) {
        return true;
    }
    
    // Check if PSRAM is available
    g_psram_available = (ESP.getPsramSize() > 0);
    
    // Initialize statistics
    g_memory_stats.total_allocated = 0;
    g_memory_stats.peak_allocated = 0;
    g_memory_stats.allocation_count = 0;
    g_memory_stats.failed_allocations = 0;
    g_alloc_list = nullptr;
    
    WEBSCREEN_LOG_INFO("Memory Manager", "Initialized - PSRAM: %s", 
                       g_psram_available ? "Available" : "Not Available");
    
    if (g_psram_available) {
        WEBSCREEN_LOG_INFO("Memory Manager", "PSRAM Size: %d bytes", ESP.getPsramSize());
    }
    
    g_memory_manager_initialized = true;
    return true;
}

static void track_allocation(void* ptr, size_t size, const char* caller) {
    if (!ptr) return;
    
    alloc_entry_t* entry = (alloc_entry_t*)malloc(sizeof(alloc_entry_t));
    if (entry) {
        entry->ptr = ptr;
        entry->size = size;
        entry->caller = caller;
        entry->timestamp = millis();
        entry->next = g_alloc_list;
        g_alloc_list = entry;
        
        g_memory_stats.total_allocated += size;
        g_memory_stats.allocation_count++;
        
        if (g_memory_stats.total_allocated > g_memory_stats.peak_allocated) {
            g_memory_stats.peak_allocated = g_memory_stats.total_allocated;
        }
    }
}

static void untrack_allocation(void* ptr) {
    if (!ptr) return;
    
    alloc_entry_t** current = &g_alloc_list;
    while (*current) {
        if ((*current)->ptr == ptr) {
            alloc_entry_t* to_remove = *current;
            g_memory_stats.total_allocated -= to_remove->size;
            g_memory_stats.allocation_count--;
            *current = to_remove->next;
            free(to_remove);
            return;
        }
        current = &(*current)->next;
    }
}

void* memory_alloc(size_t size, memory_strategy_t strategy, const char* caller) {
    if (!g_memory_manager_initialized) {
        memory_manager_init();
    }
    
    void* ptr = nullptr;
    
    switch (strategy) {
        case MEMORY_STRATEGY_INTERNAL_ONLY:
            ptr = heap_caps_malloc(size, MALLOC_CAP_INTERNAL);
            break;
            
        case MEMORY_STRATEGY_PSRAM_ONLY:
            if (g_psram_available) {
                ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
            }
            break;
            
        case MEMORY_STRATEGY_PSRAM_PREFERRED:
            if (g_psram_available) {
                ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
            }
            if (!ptr) {
                ptr = heap_caps_malloc(size, MALLOC_CAP_INTERNAL);
            }
            break;
            
        case MEMORY_STRATEGY_AUTO:
        default:
            // For large allocations (>4KB), prefer PSRAM if available
            if (size > 4096 && g_psram_available) {
                ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
            }
            if (!ptr) {
                ptr = heap_caps_malloc(size, MALLOC_CAP_INTERNAL);
            }
            break;
    }
    
    if (ptr) {
        track_allocation(ptr, size, caller);
        WEBSCREEN_LOG_DEBUG("Memory", "Allocated %d bytes at %p (%s)", size, ptr, caller);
    } else {
        g_memory_stats.failed_allocations++;
        WEBSCREEN_LOG_ERROR("Memory", "Failed to allocate %d bytes (%s)", size, caller);
    }
    
    return ptr;
}

void memory_free(void* ptr, const char* caller) {
    if (!ptr) return;
    
    untrack_allocation(ptr);
    heap_caps_free(ptr);
    WEBSCREEN_LOG_DEBUG("Memory", "Freed memory at %p (%s)", ptr, caller);
}

void* memory_realloc(void* ptr, size_t new_size, memory_strategy_t strategy, const char* caller) {
    if (!ptr) {
        return memory_alloc(new_size, strategy, caller);
    }
    
    if (new_size == 0) {
        memory_free(ptr, caller);
        return nullptr;
    }
    
    // For realloc, we need to untrack the old allocation and track the new one
    untrack_allocation(ptr);
    
    void* new_ptr = nullptr;
    
    switch (strategy) {
        case MEMORY_STRATEGY_INTERNAL_ONLY:
            new_ptr = heap_caps_realloc(ptr, new_size, MALLOC_CAP_INTERNAL);
            break;
            
        case MEMORY_STRATEGY_PSRAM_PREFERRED:
            if (g_psram_available) {
                new_ptr = heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM);
            }
            if (!new_ptr) {
                new_ptr = heap_caps_realloc(ptr, new_size, MALLOC_CAP_INTERNAL);
            }
            break;
            
        case MEMORY_STRATEGY_AUTO:
        default:
            if (new_size > 4096 && g_psram_available) {
                new_ptr = heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM);
            }
            if (!new_ptr) {
                new_ptr = heap_caps_realloc(ptr, new_size, MALLOC_CAP_INTERNAL);
            }
            break;
    }
    
    if (new_ptr) {
        track_allocation(new_ptr, new_size, caller);
        WEBSCREEN_LOG_DEBUG("Memory", "Reallocated %d bytes at %p (%s)", new_size, new_ptr, caller);
    } else {
        // If realloc failed, the original pointer is still valid, so track it again
        track_allocation(ptr, 0, caller); // Size unknown, but preserve tracking
        g_memory_stats.failed_allocations++;
        WEBSCREEN_LOG_ERROR("Memory", "Failed to reallocate %d bytes (%s)", new_size, caller);
    }
    
    return new_ptr;
}

void memory_get_stats(memory_stats_t* stats) {
    if (!stats) return;
    
    *stats = g_memory_stats;
    stats->internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    stats->psram_free = g_psram_available ? heap_caps_get_free_size(MALLOC_CAP_SPIRAM) : 0;
}

void memory_print_report(void) {
    memory_stats_t stats;
    memory_get_stats(&stats);
    
    WEBSCREEN_LOG_INFO("Memory Report", "=== MEMORY USAGE REPORT ===");
    WEBSCREEN_LOG_INFO("Memory Report", "Currently allocated: %d bytes", stats.total_allocated);
    WEBSCREEN_LOG_INFO("Memory Report", "Peak allocation: %d bytes", stats.peak_allocated);
    WEBSCREEN_LOG_INFO("Memory Report", "Active allocations: %d", stats.allocation_count);
    WEBSCREEN_LOG_INFO("Memory Report", "Failed allocations: %d", stats.failed_allocations);
    WEBSCREEN_LOG_INFO("Memory Report", "Internal RAM free: %d bytes", stats.internal_free);
    
    if (g_psram_available) {
        WEBSCREEN_LOG_INFO("Memory Report", "PSRAM free: %d bytes", stats.psram_free);
    } else {
        WEBSCREEN_LOG_INFO("Memory Report", "PSRAM: Not available");
    }
    
    // Print active allocations (for debugging leaks)
    if (g_alloc_list) {
        WEBSCREEN_LOG_INFO("Memory Report", "=== ACTIVE ALLOCATIONS ===");
        alloc_entry_t* current = g_alloc_list;
        int count = 0;
        while (current && count < 10) { // Limit to prevent spam
            WEBSCREEN_LOG_INFO("Memory Report", "  %p: %d bytes (%s) age:%dms", 
                              current->ptr, current->size, current->caller,
                              millis() - current->timestamp);
            current = current->next;
            count++;
        }
        if (current) {
            WEBSCREEN_LOG_INFO("Memory Report", "  ... and more");
        }
    }
    
    WEBSCREEN_LOG_INFO("Memory Report", "=== END REPORT ===");
}

bool memory_psram_available(void) {
    return g_psram_available;
}

memory_strategy_t memory_get_recommended_strategy(size_t required_size) {
    memory_stats_t stats;
    memory_get_stats(&stats);
    
    // If PSRAM not available, use internal only
    if (!g_psram_available) {
        return MEMORY_STRATEGY_INTERNAL_ONLY;
    }
    
    // For very large allocations, prefer PSRAM
    if (required_size > 32768) { // 32KB
        return MEMORY_STRATEGY_PSRAM_PREFERRED;
    }
    
    // If internal RAM is low, prefer PSRAM
    if (stats.internal_free < required_size * 2) {
        return MEMORY_STRATEGY_PSRAM_PREFERRED;
    }
    
    // For small allocations, use internal RAM (faster)
    if (required_size < 1024) { // 1KB
        return MEMORY_STRATEGY_INTERNAL_ONLY;
    }
    
    // Default to auto strategy
    return MEMORY_STRATEGY_AUTO;
}