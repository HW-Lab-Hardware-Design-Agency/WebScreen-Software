# Migration Guide: WebScreen v1.x to v2.0

This guide helps you migrate from WebScreen v1.x to the new v2.0 architecture.

## 📋 Overview of Changes

WebScreen v2.0 is a complete architectural rewrite focused on:
- **Modularity**: Better code organization and separation of concerns
- **Performance**: Optimized memory management and rendering
- **Reliability**: Comprehensive error handling and recovery
- **Developer Experience**: Better debugging and contribution tools

## 🔄 Breaking Changes

### Configuration File Format

#### v1.x Configuration
```json
{
  "settings": {
    "wifi": {
      "ssid": "MyNetwork",
      "pass": "MyPassword"
    },
    "mqtt": {
      "enabled": false
    }
  },
  "screen": {
    "background": "#2980b9",
    "foreground": "#00fff1"
  },
  "script": "app.js"
}
```

#### v2.0 Configuration
```json
{
  "wifi": {
    "ssid": "MyNetwork", 
    "password": "MyPassword",
    "enabled": true,
    "connection_timeout": 15000,
    "auto_reconnect": true
  },
  "mqtt": {
    "enabled": false,
    "broker": "",
    "port": 1883,
    "username": "",
    "password": "",
    "client_id": "webscreen_001"
  },
  "display": {
    "brightness": 200,
    "rotation": 1,
    "background_color": "#2980b9",
    "foreground_color": "#00fff1",
    "screen_timeout": 0
  },
  "system": {
    "device_name": "WebScreen",
    "log_level": 2,
    "performance_mode": false
  },
  "script_file": "/app.js"
}
```

### Migration Script

Use this script to convert your v1.x configuration:

```javascript
// config_migration.js - Run this in your JavaScript app
function migrateConfigV1toV2(oldConfig) {
  return {
    wifi: {
      ssid: oldConfig.settings?.wifi?.ssid || "",
      password: oldConfig.settings?.wifi?.pass || "",
      enabled: true,
      connection_timeout: 15000,
      auto_reconnect: true
    },
    mqtt: {
      enabled: oldConfig.settings?.mqtt?.enabled || false,
      broker: "",
      port: 1883,
      username: "",
      password: "",
      client_id: "webscreen_001"
    },
    display: {
      brightness: 200,
      rotation: 1,
      background_color: oldConfig.screen?.background || "#000000",
      foreground_color: oldConfig.screen?.foreground || "#FFFFFF",
      screen_timeout: 0
    },
    system: {
      device_name: "WebScreen",
      log_level: 2,
      performance_mode: false
    },
    script_file: "/" + (oldConfig.script || "app.js")
  };
}
```

## 📁 File System Changes

### v1.x Structure
```
SD Card Root/
├── webscreen.json
├── app.js
├── cert.pem
└── data/
```

### v2.0 Structure (Recommended)
```
SD Card Root/
├── webscreen.json          # Main configuration
├── webscreen.log           # System logs (auto-generated)
├── apps/                   # JavaScript applications
│   ├── weather.js
│   ├── clock.js
│   └── dashboard.js
├── certs/                  # SSL certificates
│   ├── api_cert.pem
│   └── mqtt_cert.pem
├── assets/                 # Static assets
│   ├── images/
│   └── fonts/
└── config/                 # Additional configs
    └── backup/
        └── webscreen.json.bak
```

## 🔧 API Changes

### Logging Functions

#### v1.x
```cpp
LOG("Message");
LOGF("Format %s", value);
```

#### v2.0
```cpp
// Still supported for backward compatibility
LOG("Message");
LOGF("Format %s", value);

// New structured logging (recommended)
WEBSCREEN_LOG_INFO("Module", "Message");
WEBSCREEN_LOG_ERROR("Module", "Error: %s", error);
```

### Memory Allocation

#### v1.x
```cpp
void* ptr = malloc(size);
void* psram_ptr = ps_malloc(size);
```

#### v2.0
```cpp
// Recommended: Use memory manager
void* ptr = MEMORY_ALLOC(size);
void* psram_ptr = MEMORY_ALLOC_PSRAM(size);

// When done
MEMORY_FREE(ptr);
```

### Global Variables

#### v1.x
```cpp
extern String g_script_filename;
extern bool g_mqtt_enabled;
extern uint32_t g_bg_color;
extern uint32_t g_fg_color;
```

#### v2.0
```cpp
// Access via configuration manager
webscreen_config_t* config = config_manager_get_config();
String script_file = config->script_file;
bool mqtt_enabled = config->mqtt.enabled;
uint32_t bg_color = config->display.background_color;
```

## 🚀 Performance Improvements

### PSRAM Usage

v2.0 includes intelligent PSRAM management:

```cpp
// v2.0 automatically chooses best allocation strategy
void* buffer = MEMORY_ALLOC(large_size);  // Uses PSRAM if available

// For specific control
void* internal_buffer = MEMORY_ALLOC_INTERNAL(size);  // Internal RAM only
void* psram_buffer = MEMORY_ALLOC_PSRAM(size);        // PSRAM preferred
```

### Display Optimization

```cpp
// v2.0 includes performance monitoring
display_set_performance_monitoring(true);

display_stats_t stats;
display_get_stats(&stats);
Serial.printf("FPS: %d\n", stats.last_fps);
```

## 🐛 Error Handling

### v1.x Error Handling
```cpp
if (!init_function()) {
  Serial.println("Init failed");
  return false;
}
```

### v2.0 Error Handling
```cpp
if (!init_function()) {
  WEBSCREEN_ERROR_REPORT_ERROR(WEBSCREEN_ERROR_INIT_FAILED, 
                               "Initialization failed");
  return false;
}

// System automatically handles recovery strategies
```

## 📊 Migration Checklist

### Pre-Migration
- [ ] Backup your current SD card contents
- [ ] Note your current configuration settings
- [ ] Test your JavaScript applications work in v1.x
- [ ] Check available storage space (v2.0 requires ~100KB additional space)

### Migration Steps
1. **Update Hardware**
   - [ ] Flash v2.0 firmware to your WebScreen device
   - [ ] Verify serial output shows "WebScreen v2.0"

2. **Convert Configuration**
   - [ ] Convert `webscreen.json` using migration script
   - [ ] Update any hardcoded paths in your JavaScript apps
   - [ ] Test configuration loads without errors

3. **Update File Structure**
   - [ ] Create recommended directory structure
   - [ ] Move JavaScript files to `/apps/` directory
   - [ ] Update `script_file` path in configuration

4. **Test Functionality**
   - [ ] Verify WiFi connection works
   - [ ] Test display rendering and power button
   - [ ] Check JavaScript application execution
   - [ ] Verify error handling and logging

### Post-Migration
- [ ] Monitor system health using new logging features
- [ ] Optimize configuration for your use case
- [ ] Update any external documentation or guides
- [ ] Consider contributing improvements back to the project

## 🔍 Troubleshooting

### Common Migration Issues

#### Configuration Not Loading
```
Error: Failed to parse configuration
```
**Solution**: Use the migration script to convert your v1.x config format.

#### JavaScript App Not Found
```
Error: Script file not found
```
**Solution**: Update the `script_file` path to include leading slash (e.g., `/apps/weather.js`).

#### Memory Issues
```
Error: Failed to allocate display buffer
```
**Solution**: v2.0 has better memory management. Check if PSRAM is enabled in board settings.

#### Performance Degradation
```
Warning: Low frame rate detected
```
**Solution**: Enable performance mode in system configuration:
```json
{
  "system": {
    "performance_mode": true
  }
}
```

### Debugging Migration Issues

1. **Enable Debug Logging**
   ```json
   {
     "system": {
       "log_level": 0
     }
   }
   ```

2. **Check Memory Usage**
   ```cpp
   memory_print_report();  // In your JavaScript or via serial
   ```

3. **Monitor System Health**
   ```cpp
   webscreen_error_print_report();  // Shows any system errors
   ```

## 📞 Getting Help

If you encounter issues during migration:

1. **Check the logs**: v2.0 provides detailed logging to help identify issues
2. **Search existing issues**: Look for similar problems on GitHub
3. **Create an issue**: Include your configuration, logs, and steps to reproduce
4. **Join discussions**: Use GitHub Discussions for migration questions

## 🎯 Next Steps

After successful migration:

1. **Explore new features**: Try the enhanced JavaScript APIs
2. **Optimize performance**: Use the new configuration options
3. **Contribute back**: Report issues or suggest improvements
4. **Update your documentation**: Reflect the new v2.0 features

Remember: v2.0 is designed to be more robust and maintainable. While migration requires some effort, the improved architecture provides better long-term stability and performance.