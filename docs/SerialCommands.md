# WebScreen Serial Commands Reference

This document provides a comprehensive guide to WebScreen's interactive serial command system, introduced in version 2.0. The serial command system transforms WebScreen from a simple display device into a complete embedded development platform.

## Overview

The serial command system allows developers to interact with WebScreen through a command-line interface accessible via the serial monitor. All commands start with a forward slash (`/`) and are available in both fallback mode and while running JavaScript applications.

### Key Benefits
- **Eliminates SD Card Workflow**: No more constant SD card insertion/removal cycles
- **Real-time Development**: Write, test, and debug JavaScript applications interactively
- **System Monitoring**: Live access to memory, storage, and network statistics
- **Configuration Management**: Dynamic configuration changes without file editing
- **Universal Availability**: Commands work in all operating modes

## Getting Started

### Connection Setup
1. Connect WebScreen via USB-C cable
2. Open serial monitor at 115200 baud
3. You should see the WebScreen console prompt:
   ```
   === WebScreen Serial Console ===
   Type /help for available commands
   
   WebScreen>
   ```

### Basic Usage
- Type commands starting with `/`
- Press Enter to execute
- Use `/help` to see all available commands
- Commands are case-insensitive

## Command Reference

### Core System Commands

#### `/help` or `/h`
Displays the complete list of available commands with usage examples.

**Usage:**
```
WebScreen> /help
```

**Output:**
```
=== WebScreen Commands ===
/help                    - Show this help
/stats                   - Show system statistics
/info                    - Show device information
/write <filename>        - Write JS script to SD card (interactive)
/config get <key>        - Get config value from webscreen.json
/config set <key> <val>  - Set config value in webscreen.json
/ls [path]               - List files/directories
/cat <file>              - Display file contents
/rm <file>               - Delete file
/load <script.js>        - Load/switch to different JS app
/reboot                  - Restart the device

Examples:
/write hello.js
/config get wifi.ssid
/config set wifi.ssid MyNetwork
/ls /
/cat webscreen.json
```

#### `/stats`
Provides comprehensive system statistics including memory usage, storage information, network status, and system uptime.

**Usage:**
```
WebScreen> /stats
```

**Output:**
```
=== System Statistics ===
Free Heap: 234.5 KB
Total Heap: 320.0 KB
Free PSRAM: 7.2 MB
Total PSRAM: 8.0 MB
SD Card Size: 32.0 GB
SD Card Used: 2.4 MB
SD Card Free: 31.9 GB
WiFi: Connected to MyNetwork
IP Address: 192.168.1.100
Signal Strength: -45 dBm
Uptime: 3247 seconds
CPU Frequency: 240 MHz
```

**Use Cases:**
- Monitor memory usage during development
- Check SD card space before deploying applications
- Verify network connectivity status
- Debug memory leaks in JavaScript applications

#### `/info`
Displays detailed device information including hardware specifications, firmware version, and build details.

**Usage:**
```
WebScreen> /info
```

**Output:**
```
=== Device Information ===
Chip Model: ESP32-S3
Chip Revision: 0
Flash Size: 16.0 MB
Flash Speed: 80 MHz
MAC Address: 24:6F:28:12:34:56
SDK Version: v4.4.2
WebScreen Version: 2.0.0
Build Date: Dec 15 2024 14:30:25
```

#### `/reboot` or `/restart`
Restarts the WebScreen device. Useful for applying configuration changes or recovering from errors.

**Usage:**
```
WebScreen> /reboot
[OK] Rebooting in 3 seconds...
```

**Warning:** The device will restart immediately after the 3-second delay.

### Script Management Commands

#### `/write <filename>`
Interactive JavaScript editor that allows you to write scripts directly through the serial interface.

**Usage:**
```
WebScreen> /write weather.js
Enter JavaScript code. End with a line containing only 'END':
---
+ // Weather display application
+ let temp = http_get('https://api.weather.com/current');
+ let data = parse_json_value(temp, 'temperature');
+ create_label_with_text('Temperature: ' + data + '°C');
+ END
[OK] Script saved: /weather.js (234 bytes)
```

**Features:**
- **Line-by-line Input**: Each line is echoed with a `+` prefix for confirmation
- **Auto Extension**: Automatically adds `.js` extension if not provided
- **Size Reporting**: Shows file size after successful save
- **Error Handling**: Provides clear error messages for SD card issues

**Best Practices:**
- Plan your code structure before starting
- Use meaningful variable names for readability
- Test with small scripts first
- Remember to type `END` exactly to finish

#### `/load <script.js>` or `/run <script.js>`
Switches to a different JavaScript application without manually editing configuration files.

**Usage:**
```
WebScreen> /load weather.js
[OK] Script queued for loading: /weather.js
[OK] Restarting to load new script...
```

**Features:**
- **Auto Extension**: Adds `.js` extension automatically
- **File Validation**: Checks if script exists before switching
- **Graceful Restart**: Cleanly stops current application and restarts
- **Global Update**: Updates the global script filename for future boots

**Use Cases:**
- Switch between different applications for testing
- Deploy new versions without SD card removal
- A/B testing of application variants
- Quick application switching for demonstrations

### Configuration Management

#### `/config get <key>`
Retrieves values from the `webscreen.json` configuration file.

**Usage:**
```
WebScreen> /config get wifi.ssid
wifi.ssid = MyHomeNetwork

WebScreen> /config get display.brightness
display.brightness = 200
```

**Supported Key Formats:**
- **Nested Keys**: Use dot notation (e.g., `wifi.ssid`, `display.brightness`)
- **Root Keys**: Direct access to top-level keys (e.g., `script_file`)

**Common Configuration Keys:**
```
wifi.ssid                 - WiFi network name
wifi.password             - WiFi password
display.brightness        - Screen brightness (0-255)
display.background_color  - Background color (hex format)
display.foreground_color  - Text color (hex format)
script_file               - Default JavaScript application
mqtt.enabled              - MQTT feature toggle
system.device_name        - Device identifier
```

#### `/config set <key> <value>`
Updates configuration values and saves them to the JSON file.

**Usage:**
```
WebScreen> /config set wifi.ssid OfficeNetwork
[OK] Config updated: wifi.ssid = OfficeNetwork

WebScreen> /config set display.brightness 150
[OK] Config updated: display.brightness = 150
```

**Features:**
- **Nested Key Support**: Automatically handles JSON structure creation
- **Type Preservation**: Maintains appropriate data types (string, number, boolean)
- **Immediate Save**: Changes are written to SD card immediately
- **JSON Formatting**: Maintains pretty-printed JSON structure

**Important Notes:**
- Configuration changes may require a reboot to take effect
- Always verify changes with `/config get` before rebooting
- Backup your configuration file before making extensive changes

### File System Operations

#### `/ls [path]` or `/list [path]`
Lists files and directories on the SD card.

**Usage:**
```
WebScreen> /ls /
Directory listing for: /
Type    Size        Name
--------------------------------
DIR                 apps
FILE    1.2 KB      webscreen.json
FILE    456 B       hello.js
FILE    2.3 KB      weather.js
DIR                 certificates
FILE    15.6 KB     webscreen.gif

WebScreen> /ls /apps
Directory listing for: /apps
Type    Size        Name
--------------------------------
FILE    3.4 KB      dashboard.js
FILE    1.8 KB      clock.js
FILE    2.1 KB      notifications.js
```

**Default Path:** If no path is specified, lists the root directory (`/`)

**Output Format:**
- **Type**: `FILE` or `DIR`
- **Size**: File size in human-readable format (KB, MB, GB)
- **Name**: File or directory name

#### `/cat <file>` or `/view <file>`
Displays the contents of a file.

**Usage:**
```
WebScreen> /cat hello.js

--- /hello.js ---
// Simple hello world application
create_label_with_text('Hello WebScreen!');
set_background_color('#2980b9');
--- End of file ---
```

**Features:**
- **Automatic Path Resolution**: Adds leading slash if not provided
- **Clear Delimiters**: Shows file boundaries with clear markers
- **Error Handling**: Provides helpful error messages for missing files

**Use Cases:**
- Verify script contents before loading
- Check configuration files
- Debug file formatting issues
- Review log files

#### `/rm <file>` or `/delete <file>`
Deletes a file from the SD card.

**Usage:**
```
WebScreen> /rm old_script.js
[OK] File deleted: /old_script.js

WebScreen> /rm debug.log
[OK] File deleted: /debug.log
```

**Safety Features:**
- **Confirmation Messages**: Clear feedback on successful deletion
- **Error Reporting**: Helpful error messages if deletion fails
- **Path Normalization**: Handles paths with or without leading slashes

**Warning:** File deletion is permanent. Ensure you have backups if needed.

## Advanced Usage Patterns

### Rapid Prototyping Workflow

**1. Create and Test Script:**
```
WebScreen> /write prototype.js
Enter JavaScript code. End with a line containing only 'END':
---
+ create_label_with_text('Prototype v1');
+ END
[OK] Script saved: /prototype.js (32 bytes)

WebScreen> /load prototype.js
[OK] Script queued for loading: /prototype.js
[OK] Restarting to load new script...
```

**2. Monitor Resources:**
```
WebScreen> /stats
=== System Statistics ===
Free Heap: 245.2 KB
[... monitoring output ...]
```

**3. Iterate Quickly:**
```
WebScreen> /write prototype.js
Enter JavaScript code. End with a line containing only 'END':
---
+ create_label_with_text('Prototype v2 - Improved!');
+ END
[OK] Script saved: /prototype.js (45 bytes)
```

### A/B Testing Pattern

**Setup Multiple Variants:**
```
WebScreen> /write version_a.js
[... enter code for version A ...]

WebScreen> /write version_b.js
[... enter code for version B ...]
```

**Quick Switching:**
```
WebScreen> /load version_a.js
[... test version A ...]

WebScreen> /load version_b.js
[... test version B ...]
```

**Compare Performance:**
```
WebScreen> /stats
[... monitor memory usage for each version ...]
```

### Configuration Management Pattern

**Network Setup:**
```
WebScreen> /config get wifi.ssid
wifi.ssid = OldNetwork

WebScreen> /config set wifi.ssid NewNetwork
[OK] Config updated: wifi.ssid = NewNetwork

WebScreen> /config set wifi.password NewPassword123
[OK] Config updated: wifi.password = NewPassword123

WebScreen> /reboot
[OK] Rebooting in 3 seconds...
```

**Display Customization:**
```
WebScreen> /config set display.brightness 255
[OK] Config updated: display.brightness = 255

WebScreen> /config set display.background_color #1a1a1a
[OK] Config updated: display.background_color = #1a1a1a
```

### File Organization Pattern

**Create Directory Structure:**
```
WebScreen> /ls /
[... see current structure ...]

WebScreen> /write apps/weather/main.js
[... create organized application structure ...]

WebScreen> /write apps/clock/display.js
[... create another organized app ...]

WebScreen> /ls /apps
[... verify organization ...]
```

## Troubleshooting

### Common Issues

#### Command Not Recognized
**Symptom:**
```
WebScreen> help
[ERROR] Commands must start with '/'. Type /help for help.
```

**Solution:** Always prefix commands with `/`

#### SD Card Not Available
**Symptom:**
```
WebScreen> /ls
[ERROR] SD card not available
```

**Solutions:**
- Check SD card insertion
- Verify SD card format (should be FAT32)
- Try reinserting SD card
- Use `/reboot` to reinitialize

#### Script Not Found
**Symptom:**
```
WebScreen> /load missing.js
[ERROR] Script not found: /missing.js
```

**Solutions:**
- Use `/ls` to verify file exists
- Check filename spelling and extension
- Verify file was saved correctly with `/cat`

#### Configuration Key Not Found
**Symptom:**
```
WebScreen> /config get invalid.key
[ERROR] Key not found: invalid.key
```

**Solutions:**
- Use `/cat webscreen.json` to see available keys
- Check key spelling and dot notation
- Verify configuration file structure

### Performance Considerations

#### Memory Management
- Use `/stats` regularly to monitor heap usage
- Be aware of memory consumption in JavaScript applications
- Consider script size when using `/write` for large applications

#### Storage Management
- Monitor SD card space with `/stats`
- Clean up old files with `/rm`
- Organize files in directories for better management

#### Network Operations
- Check WiFi connectivity with `/stats`
- Verify network configuration with `/config get`
- Use `/reboot` after network configuration changes

## Integration with Development Tools

### Serial Monitor Setup

**Arduino IDE:**
1. Tools → Serial Monitor
2. Set baud rate to 115200
3. Set line ending to "Newline"

**PlatformIO:**
1. Use built-in serial monitor
2. Configure baud rate in platformio.ini
3. Use Ctrl+Shift+P → "Serial Monitor"

**Third-party Tools:**
- PuTTY (Windows)
- Screen (Linux/Mac)
- Minicom (Linux)
- CoolTerm (Cross-platform)

### Automation Possibilities

The command system can be automated using serial communication libraries in various programming languages:

**Python Example:**
```python
import serial
import time

# Connect to WebScreen
ser = serial.Serial('/dev/ttyUSB0', 115200, timeout=1)
time.sleep(2)

# Send commands
ser.write(b'/stats\n')
response = ser.readline().decode()
print(response)

# Upload a script
ser.write(b'/write auto_generated.js\n')
ser.write(b'create_label_with_text("Automated deployment");\n')
ser.write(b'END\n')

ser.close()
```

**Node.js Example:**
```javascript
const SerialPort = require('serialport');
const port = new SerialPort('/dev/ttyUSB0', { baudRate: 115200 });

port.write('/stats\n');
port.on('data', (data) => {
  console.log('Data:', data.toString());
});
```

## Command Implementation Details

### Technical Architecture
- Commands are processed in both `fallback.cpp` and `dynamic_js.cpp`
- Command parsing uses Arduino String class for simplicity
- JSON configuration uses ArduinoJson library
- File operations use ESP32 SD_MMC driver
- Memory formatting uses custom byte formatting functions

### Security Considerations
- Commands require physical serial connection
- No remote command execution capability
- File operations are limited to SD card
- Configuration changes are immediately persisted

### Performance Impact
- Command processing is non-blocking
- File operations may cause brief UI pauses
- Large file transfers should be chunked
- Memory allocation is carefully managed

## Future Enhancements

### Planned Features
- **Batch Commands**: Execute multiple commands from a file
- **Command History**: Navigate previous commands with arrow keys
- **Tab Completion**: Auto-complete file names and paths
- **Remote Access**: Execute commands over WiFi connection
- **Script Templates**: Pre-built application templates
- **Backup/Restore**: Full configuration and script backup

### Community Contributions
- Submit feature requests via GitHub issues
- Contribute command implementations
- Improve error handling and user experience
- Add automation tools and integrations

## Conclusion

The WebScreen serial command system transforms embedded development by providing immediate, interactive access to all device functions. This eliminates traditional barriers like SD card management and enables rapid iteration during application development.

Whether you're prototyping a new idea, debugging an existing application, or managing device configurations, the command system provides the tools needed for efficient WebScreen development.

For additional support and examples, visit the [WebScreen GitHub repository](https://github.com/HW-Lab-Hardware-Design-Agency/WebScreen-Software) or check out the [complete API documentation](API.md).