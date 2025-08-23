# Contributing to WebScreen

Thank you for your interest in contributing to WebScreen! This document provides guidelines and information for contributors.

## Table of Contents

- [Development Environment Setup](#development-environment-setup)
- [Code Architecture](#code-architecture)
- [Coding Standards](#coding-standards)
- [Testing Guidelines](#testing-guidelines)
- [Submitting Changes](#submitting-changes)
- [Debugging and Troubleshooting](#debugging-and-troubleshooting)
- [Performance Guidelines](#performance-guidelines)

## Development Environment Setup

### Requirements

- **Arduino IDE** 2.0 or newer
- **ESP32 Arduino Core** version 2.0.3 or newer  
- **WebScreen custom PCB** with ESP32-S3
- **SD Card** (Class 10 recommended) for testing
- **USB-C cable** for programming and debugging

### Initial Setup

1. **Clone the repository:**
   ```bash
   git clone https://github.com/HW-Lab-Hardware-Design-Agency/WebScreen-Software.git
   cd WebScreen-Software
   ```

2. **Install ESP32 board support:**
   - Arduino IDE: Add `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json` to Board Manager URLs
   - Install ESP32 boards package version 2.0.3+

3. **Install required libraries via Arduino Library Manager:**
   - ArduinoJson (by Benoit Blanchon)
   - LVGL (by kisvegabor)
   - PubSubClient (by Nick O'Leary)

4. **Configure board settings:**
   - Board: ESP32S3 Dev Module
   - CPU Frequency: 240MHz
   - Flash Size: 16MB
   - PSRAM: OPI PSRAM
   - USB CDC On Boot: Enabled

5. **Open the main sketch:**
   ```
   File → Open → WebScreen-Software/webscreen/webscreen.ino
   ```

## Code Architecture

WebScreen v2.0 follows a modular architecture for better maintainability:

```
src/
├── core/              # Core application logic
│   ├── webscreen.ino  # Arduino entry point
│   ├── config_manager # Configuration management
│   └── error_handler  # Error handling system
├── hardware/          # Hardware abstraction layer
│   ├── display/       # Display management
│   ├── storage/       # SD card and file system
│   ├── pins/          # Pin configuration
│   └── power/         # Power management
├── runtime/           # Application runtime engines
│   ├── js/            # JavaScript/Elk engine
│   ├── fallback/      # Fallback notification app
│   └── lvgl/          # LVGL integration
├── network/           # Network functionality
│   ├── wifi/          # WiFi management
│   ├── http/          # HTTP/HTTPS client
│   ├── mqtt/          # MQTT client
│   └── ble/           # Bluetooth LE
└── utils/             # Utilities and helpers
    ├── logging/       # Structured logging
    ├── memory/        # Memory management
    └── timing/        # Timing utilities
```

### Key Design Principles

1. **Separation of Concerns**: Each module has a single, well-defined responsibility
2. **Hardware Abstraction**: Hardware-specific code is isolated in HAL modules
3. **Error Handling**: Comprehensive error handling with recovery strategies
4. **Memory Safety**: Unified memory management with leak detection
5. **Performance**: Optimized for ESP32-S3 with PSRAM support
6. **Testing**: Modular design enables unit testing

## Coding Standards

### General Guidelines

- **Language**: C++ with C-style interfaces for modularity
- **Naming**: Use snake_case for functions and variables, UPPER_CASE for constants
- **Documentation**: All public functions must have Doxygen comments
- **Memory**: Always check allocation success, use RAII patterns where possible
- **Error Handling**: Use the centralized error handling system

### Code Formatting

```cpp
/**
 * @brief Function description
 * @param param1 Description of parameter
 * @return Description of return value
 */
bool example_function(int param1, const char* param2) {
    // Check input parameters
    if (!param2) {
        WEBSCREEN_ERROR_REPORT_ERROR(WEBSCREEN_ERROR_INVALID_PARAMETER, 
                                     "param2 cannot be NULL");
        return false;
    }
    
    // Allocate memory safely
    char* buffer = (char*)MEMORY_ALLOC(256);
    if (!buffer) {
        WEBSCREEN_ERROR_REPORT_ERROR(WEBSCREEN_ERROR_MEMORY_ALLOCATION_FAILED,
                                     "Failed to allocate buffer");
        return false;
    }
    
    // ... function implementation ...
    
    // Clean up
    MEMORY_FREE(buffer);
    return true;
}
```

### Header Guards and Includes

```cpp
/**
 * @file example.h
 * @brief Brief description of the file
 */

#pragma once

#include <Arduino.h>  // System includes first
#include "local.h"    // Local includes second

#ifdef __cplusplus
extern "C" {
#endif

// Declarations here

#ifdef __cplusplus
}
#endif
```

### Error Handling Patterns

```cpp
// Use the centralized error reporting system
if (!wifi_connect()) {
    WEBSCREEN_ERROR_REPORT_WARNING(WEBSCREEN_ERROR_WIFI_CONNECT_FAILED,
                                   "Failed to connect to WiFi");
    return false;
}

// Check allocations
void* ptr = MEMORY_ALLOC(size);
WEBSCREEN_CHECK_ALLOC(ptr, "Failed to allocate display buffer");

// Use early returns for error conditions
if (!initialization_required) {
    return true;  // Early return for success case
}
```

## Testing Guidelines

### Unit Testing

Create unit tests for all new modules:

```cpp
// tests/test_memory_manager.cpp
#include "unity.h"
#include "memory_manager.h"

void test_memory_allocation() {
    memory_manager_init();
    
    void* ptr = MEMORY_ALLOC(1024);
    TEST_ASSERT_NOT_NULL(ptr);
    
    MEMORY_FREE(ptr);
    
    memory_stats_t stats;
    memory_get_stats(&stats);
    TEST_ASSERT_EQUAL(0, stats.total_allocated);
}

void setUp(void) {
    // Setup code here
}

void tearDown(void) {
    // Cleanup code here
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_memory_allocation);
    return UNITY_END();
}
```

### Integration Testing

Test complete workflows:

1. **SD Card Integration**: Test configuration loading, script execution
2. **Display Integration**: Test LVGL initialization, rendering
3. **Network Integration**: Test WiFi connection, HTTP requests
4. **JavaScript Integration**: Test Elk engine, API bindings

### Hardware-in-the-Loop Testing

For testing on actual hardware:

```cpp
void test_display_functionality() {
    // Initialize display
    display_config_t config = {/* ... */};
    TEST_ASSERT_TRUE(display_manager_init(&config));
    
    // Test basic operations
    TEST_ASSERT_TRUE(display_set_brightness(128));
    TEST_ASSERT_EQUAL(128, display_get_brightness());
    
    // Test performance
    uint32_t start = millis();
    display_force_refresh();
    uint32_t duration = millis() - start;
    TEST_ASSERT_LESS_THAN(100, duration);  // Should complete in <100ms
}
```

## Submitting Changes

### Before Submitting

1. **Test thoroughly** on actual hardware
2. **Run all existing tests** to ensure no regressions
3. **Check memory usage** - no leaks, reasonable memory consumption
4. **Verify performance** - no significant performance degradation
5. **Update documentation** if adding new features

### Commit Message Format

Follow [Conventional Commits](https://www.conventionalcommits.org/):

```
type(scope): description

Body explaining the change in detail.

Fixes #123
```

Examples:
- `feat(display): add brightness control API`
- `fix(memory): resolve PSRAM allocation issue`
- `docs(api): update JavaScript API documentation`
- `perf(network): optimize HTTP request handling`

### Pull Request Process

1. **Fork the repository** and create a feature branch
2. **Implement your changes** following coding standards
3. **Add tests** for new functionality
4. **Update documentation** as needed
5. **Submit a pull request** with clear description

### Pull Request Template

```markdown
## Description
Brief description of the changes.

## Type of Change
- [ ] Bug fix (non-breaking change which fixes an issue)
- [ ] New feature (non-breaking change which adds functionality)
- [ ] Breaking change (fix or feature that would cause existing functionality to not work as expected)
- [ ] Documentation update

## Testing
- [ ] Unit tests pass
- [ ] Integration tests pass
- [ ] Tested on actual hardware
- [ ] Memory usage checked
- [ ] Performance impact assessed

## Checklist
- [ ] Code follows the style guidelines
- [ ] Self-review completed
- [ ] Documentation updated
- [ ] Tests added/updated
```

## Debugging and Troubleshooting

### Serial Debugging

Enable comprehensive logging:

```cpp
// Set debug level
webscreen_logger_set_level(WEBSCREEN_LOG_LEVEL_DEBUG);

// Log system information
webscreen_logger_log_system_info();

// Monitor memory usage
memory_print_report();
```

### Common Issues

1. **Memory Allocation Failures**
   - Check available heap: `ESP.getFreeHeap()`
   - Monitor PSRAM usage: `ESP.getFreePsram()`
   - Use memory manager statistics

2. **Display Issues**
   - Verify pin connections
   - Check LVGL buffer allocation
   - Monitor refresh rate

3. **SD Card Problems**
   - Test different SD cards
   - Check pin configuration
   - Verify file system format

4. **JavaScript Runtime Issues**
   - Check script syntax
   - Monitor Elk heap usage
   - Verify API bindings

### Debug Tools

```cpp
// Memory debugging
#define WEBSCREEN_DEBUG_MEMORY 1

// Performance profiling
#define WEBSCREEN_PROFILE_ENABLED 1

// Network debugging  
#define WEBSCREEN_DEBUG_NETWORK 1
```

## Performance Guidelines

### Memory Usage

- **Prefer stack allocation** for small, temporary objects
- **Use PSRAM** for large buffers (display, network, etc.)
- **Pool allocations** where possible to reduce fragmentation
- **Monitor peak usage** and optimize accordingly

### CPU Performance

- **Avoid blocking operations** in main loop
- **Use hardware acceleration** (DMA, etc.) where available
- **Optimize critical paths** (display rendering, network I/O)
- **Profile regularly** to identify bottlenecks

### Example Performance Optimization

```cpp
// Before: Blocking network request
String response = http_get("https://api.example.com/data");
process_response(response);

// After: Non-blocking with callback
http_get_async("https://api.example.com/data", [](const String& response) {
    process_response(response);
});
```

### Memory Pool Example

```cpp
// Use memory pools for frequent allocations
typedef struct {
    char data[256];
    bool in_use;
} buffer_pool_entry_t;

static buffer_pool_entry_t g_buffer_pool[10];

char* get_buffer_from_pool() {
    for (int i = 0; i < 10; i++) {
        if (!g_buffer_pool[i].in_use) {
            g_buffer_pool[i].in_use = true;
            return g_buffer_pool[i].data;
        }
    }
    return nullptr;  // Pool exhausted
}
```

## Getting Help

- **Documentation**: Check docs/ directory for API references
- **Issues**: Search existing issues on GitHub
- **Discussions**: Use GitHub Discussions for questions
- **Community**: Join the WebScreen community forums

## Code of Conduct

Please read and follow our [Code of Conduct](CODE_OF_CONDUCT.md).

## License

By contributing to WebScreen, you agree that your contributions will be licensed under the MIT License.