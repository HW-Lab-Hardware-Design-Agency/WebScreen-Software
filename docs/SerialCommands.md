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
/upload <file> [base64]  - Upload any file (text or base64-encoded)
/config get <key>        - Get config value from webscreen.json
/config set <key> <val>  - Set config value in webscreen.json
/ls [path] [json]        - List files/directories (json = machine-readable)
/cat <file>              - Display file contents
/rm <file|empty-dir>     - Delete file or empty directory
/mkdir <path>            - Create directory on SD card
/download <file>         - Dump file as base64 (host-side download)
/load <script.js> [save] - Load/switch to different JS app (save = persist to config)
/restart_app             - Restart the JS app in place (no reboot)
/eval <js-code>          - Evaluate JS in the running app (REPL)
/errors                  - Show last JS error and restart-ladder state
/gc                      - Run JS garbage collection
/screenshot              - Capture the screen as base64 RGB565
/wget <url> [file]       - Download file from URL to SD card
/ping <host>             - Test network connectivity
/backup [save|restore]   - Backup/restore configuration
/monitor [cpu|mem|net]   - Live system monitoring
/brightness <0-255>      - Set display brightness
/time                    - Show current device time
/settime <epoch> [tz]    - Set device time from epoch
/factory_reset confirm   - Delete webscreen.json and reboot to fallback
/reboot                  - Restart the device

Examples:
/write hello.js
/upload image.png base64
/upload config.json
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
Heap Low Watermark: 180.2 KB
Largest Free Block: 110.0 KB
JS Arena Used: 42.3 KB
JS Arena Total: 256.0 KB
SD Card Size: 32.0 GB
SD Card Used: 2.4 MB
SD Card Free: 31.9 GB
WiFi: Connected to MyNetwork
IP Address: 192.168.1.100
Signal Strength: -45 dBm
Uptime: 3247 seconds
CPU Frequency: 240 MHz
```

**Memory Fields:**
- **Heap Low Watermark**: lowest internal heap level observed since boot (high-water mark of memory pressure)
- **Largest Free Block**: largest contiguous internal allocation currently possible (fragmentation indicator)
- **JS Arena Used/Total**: Elk JavaScript heap usage (shows `JS Arena: Not running` if the engine is not active)

**Use Cases:**
- Monitor memory usage during development
- Check SD card space before deploying applications
- Verify network connectivity status
- Debug memory leaks in JavaScript applications (watch JS Arena Used between `/gc` runs)

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
JS Arena Used: 42.3 KB
JS Arena Total: 256.0 KB
```

The JS Arena lines only appear while the JavaScript engine is running.

#### `/brightness <0-255>`
Sets or queries the display brightness level.

**Usage:**
```
WebScreen> /brightness
Current brightness: 200

WebScreen> /brightness 150
[OK] Brightness set to 150

WebScreen> /brightness 255
[OK] Brightness set to 255
```

**Parameters:**
- **No argument**: Displays the current brightness level
- **0-255**: Sets the brightness to the specified value (0 = off, 255 = maximum)

**Notes:**
- Changes take effect immediately on the AMOLED display
- To persist brightness across reboots, also set `display.brightness` in the configuration:
  ```
  /config set display.brightness 150
  ```

#### `/factory_reset confirm`
Deletes `/webscreen.json` from the SD card and reboots the device into fallback mode. The literal `confirm` argument is required — without it the command only prints a warning and does nothing.

**Usage:**
```
WebScreen> /factory_reset
[ERROR] This deletes /webscreen.json and reboots into fallback mode. Run '/factory_reset confirm' to proceed.

WebScreen> /factory_reset confirm
[OK] Configuration deleted. Rebooting into fallback mode in 3 seconds...
```

**Notes:**
- Only the configuration file is deleted — scripts and other files on the SD card are untouched
- Use `/backup save` first if you may want to restore the configuration later
- After the reboot the device runs the built-in fallback notification app

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
- **Inactivity Timeout**: Aborts after 30 seconds without input (the partial file is removed)

**Best Practices:**
- Plan your code structure before starting
- Use meaningful variable names for readability
- Test with small scripts first
- Remember to type `END` exactly to finish

#### `/upload <filename> [base64]`
Uploads any file (text or binary) to the SD card. Without the `base64` argument, lines are written as plain text; with it, each line is base64-decoded and written as binary data. End the stream with a line containing only `END`.

**Usage (text):**
```
WebScreen> /upload config.json
Upload mode: text
Target file: /config.json
Send file data. End with a line containing only 'END':
---
+ {"script": "app.js"}
+ END
[OK] File saved: /config.json (21 B)
```

**Usage (binary):**
```
WebScreen> /upload image.png base64
Upload mode: base64
Target file: /image.png
Send file data. End with a line containing only 'END':
---
[... base64 chunks, one per line ...]
END
[OK] File saved: /image.png (15.6 KB)
```

**Features:**
- **Two Modes**: plain text (default) or `base64`/`b64` for binary data
- **Progress Display**: base64 mode reports progress every ~10KB
- **Bounded Decoding**: each base64 line may decode to at most 512 bytes; an oversized chunk aborts the upload with an `[ERROR]` line and the partial file is removed
- **Inactivity Timeout**: aborts after 30 seconds without input (partial file removed)

**Error Example:**
```
[ERROR] Upload aborted: chunk exceeds 512 decoded bytes per line
[ERROR] Upload failed: chunk exceeds 512 decoded bytes per line (/image.png removed)
```

**Notes:**
- Split base64 data into lines that decode to 512 bytes or less (e.g. 684 base64 characters per line)
- After an error, the device keeps draining chunks until `END` so the stream stays in sync

#### `/load <script.js> [save]` or `/run <script.js> [save]`
Switches to a different JavaScript application by restarting the JS app **in place** — the device does not reboot.

**Usage:**
```
WebScreen> /load weather.js
[OK] Loading script: /weather.js

WebScreen> /load weather.js save
[OK] Loading script: /weather.js
[OK] Config updated: script = /weather.js
```

**Features:**
- **Auto Extension**: Adds `.js` extension automatically
- **File Validation**: Checks if script exists before switching
- **In-place Restart**: The JS task tears down the current app (timers, widgets, styles, media buffers, MQTT state), re-creates the engine over the same arena, and evaluates the new script — no reboot
- **Optional Persistence**: Adding the `save` argument also writes the script path into `webscreen.json` (the `script` key) so it loads on the next boot
- **Visible Failures**: If the new script fails to run, the error is shown on the display and over serial; after two failed restart attempts the device enters safe mode and waits for a corrected `/load` or `/restart_app`

**Notes:**
- Without `save`, the switch applies to the current session only. Use `/load weather.js save` (or `/config set script weather.js`) to change the script used on the next boot

**Use Cases:**
- Switch between different applications for testing
- Deploy new versions without SD card removal
- A/B testing of application variants
- Quick application switching for demonstrations

#### `/restart_app`
Restarts the currently loaded JavaScript application in place — same teardown and re-evaluation as `/load`, but with the current script. The device does not reboot.

**Usage:**
```
WebScreen> /restart_app
[OK] JS app restart requested (in-place, no reboot)
```

**Use Cases:**
- Recover from a wedged or out-of-memory script without power-cycling
- Re-run a script after editing it with `/write` or `/upload`
- Leave safe mode after fixing a failing script

#### `/eval <js-code>`
Evaluates a one-line JavaScript snippet **inside the running app**, sharing its globals and engine state — a live REPL for inspecting or poking at a running script.

**Usage:**
```
WebScreen> /eval print(mem_info())
Queued. Result follows as [EVAL] ...
[EVAL] {"heap_free":123456,...,"js_total":262144}

WebScreen> /eval set_brightness(120)
Queued. Result follows as [EVAL] ...
[EVAL] 120
```

**Features:**
- **Runs in the running app**: The snippet evaluates at the JS task's next safe point (never mid-callback), so it sees the same variables and widgets as the app
- **Result Prefix**: The result (or error) is printed with an `[EVAL]` prefix
- **Length Limit**: Snippets are limited to 255 characters

**Refused when:**
- The JS runtime is not running (fallback mode)
- The device is in safe mode (a parked snippet prints `[EVAL] dropped: app is parked in safe mode`)
- A previous `/eval` snippet is still in flight

**Use Cases:**
- Inspect live state (`/eval print(myCounter)`)
- Trigger a function or change a setting without editing the script
- Debug a running app interactively

#### `/errors`
Prints a JavaScript error report: the last JS error with its age, the startup error (if any), the restart-failure and auto-restart-cycle counters, the safe-mode flag, and the current script.

**Usage:**
```
WebScreen> /errors

=== JS Error Report ===
Last JS error (12s ago): timer update: 'foo' not found (line 4)
Restart failures: 0/2
Auto-restart cycles: 1/10
Safe mode: no
Script: /weather.js
```

**Captured Sources:** The last JS error is captured from script evaluation failures, timer-callback errors, button-callback errors, and `/eval` errors.

**Use Cases:**
- See why a script failed without watching the serial log live
- Check whether the device is in safe mode and how close it is to the restart-ladder limits

#### `/gc`
Runs a JavaScript garbage collection at the next safe point and reports the resulting arena usage.

**Usage:**
```
WebScreen> /gc
[OK] GC complete: JS arena 42.3 KB / 256.0 KB used

WebScreen> /gc
[ERROR] Garbage collection unavailable (JS engine not running)
```

**Use Cases:**
- Check how much of the arena is live data versus garbage
- Diagnose memory leaks: a steadily growing post-GC usage means the script is retaining objects

#### `/screenshot` or `/ss`
Captures the current screen contents and streams them over serial as base64-encoded raw RGB565 pixel data. The capture is queued and executed by the JS task at its next safe point (LVGL objects must not be touched from any other task), so it requires the JavaScript runtime to be active.

**Usage:**
```
WebScreen> /screenshot
Queued. Data follows as an '=== SCREENSHOT ... ===' block

=== SCREENSHOT 536x240 RGB565_SWAP ===
[... base64 lines, 76 chars each ...]
=== SCREENSHOT END ===
```

**Stream Format:**
- Header line: `=== SCREENSHOT <w>x<h> RGB565_SWAP ===` (width and height in pixels)
- Body: base64-encoded raw RGB565 pixel data, 76 characters per line (57 raw bytes per line, classic MIME width)
- Footer line: `=== SCREENSHOT END ===`
- `RGB565_SWAP` means the two bytes of each 16-bit pixel are swapped (`LV_COLOR_16_SWAP` is enabled in `lv_conf.h`) — swap them back on the host before interpreting the pixels as little-endian RGB565

**Refused when:**
- The JS runtime is not running (fallback mode)
- A previous capture is still in flight

**Notes:**
- Implemented with LVGL's `lv_snapshot` (`LV_USE_SNAPSHOT` is enabled in `lv_conf.h`)
- The full-screen dump is ~340KB of base64 and takes low single-digit seconds over USB-CDC; LVGL is paused for the duration (the screen content is what's being captured, so this is harmless)

**Host-side decode example (Python + Pillow):**
```python
import base64, serial
from PIL import Image

ser = serial.Serial('/dev/ttyACM0', 115200, timeout=5)
ser.write(b'/screenshot\n')
data, w, h = b'', 0, 0
while True:
    line = ser.readline().decode().strip()
    if line.startswith('=== SCREENSHOT ') and 'END' not in line:
        w, h = map(int, line.split()[2].split('x'))
    elif line == '=== SCREENSHOT END ===':
        break
    elif w and line and not line.startswith(('WebScreen>', 'Queued')):
        data += base64.b64decode(line)

swapped = bytearray(len(data))     # undo LV_COLOR_16_SWAP
swapped[0::2], swapped[1::2] = data[1::2], data[0::2]
Image.frombytes('RGB', (w, h), bytes(swapped), 'raw', 'BGR;16').save('shot.png')
```

**Use Cases:**
- Document an app's UI without photographing the display
- Automated visual regression testing of JavaScript applications
- Remote debugging of rendering issues

### Network and System Commands

#### `/wget <url> [filename]` or `/fetch <url> [filename]`
Downloads files from HTTP/HTTPS URLs directly to the SD card.

> **Alias change:** `/wget`'s alias is now `/fetch`. The previous (undocumented) alias `download` has been repurposed: `/download` is now the base64 file-dump command for host-side downloads (see [File System Operations](#file-system-operations)).

**Usage:**
```
WebScreen> /wget https://example.com/config.json
Downloading: https://example.com/config.json
Saving to: /config.json
Content-Length: 2.3 KB
Progress: 10% 20% 30% 40% 50% 60% 70% 80% 90% 100% 
[OK] Downloaded 2.3 KB to /config.json
```

**Features:**
- **Auto Filename**: Extracts filename from URL if not specified
- **Progress Display**: Shows download progress for known file sizes
- **HTTPS Support**: Handles both HTTP and HTTPS protocols
- **Error Handling**: Clear error messages for connection failures

**Use Cases:**
- Download JavaScript libraries or frameworks
- Fetch configuration files from servers
- Update application scripts from GitHub
- Download assets like fonts or data files

#### `/ping <host>`
Tests network connectivity to a specified host.

**Usage:**
```
WebScreen> /ping google.com
PING google.com
Pinging google.com (142.250.185.78) with 32 bytes of data:
Reply from 142.250.185.78: time=23ms
Reply from 142.250.185.78: time=19ms
Reply from 142.250.185.78: time=21ms
Reply from 142.250.185.78: time=18ms

Ping statistics for 142.250.185.78:
    Packets: Sent = 4, Received = 4, Lost = 0 (0% loss)
Approximate round trip times:
    Minimum = 18ms, Maximum = 23ms, Average = 20ms
```

**Features:**
- **DNS Resolution**: Resolves hostnames to IP addresses
- **Statistics**: Provides min/max/average response times
- **Packet Loss**: Shows connection reliability
- **TCP-based**: Uses TCP connections for compatibility

**Use Cases:**
- Verify network connectivity before API calls
- Test DNS resolution
- Debug network issues
- Monitor connection quality

#### `/backup [save|restore|list] [name]`
Manages configuration backups with metadata tracking.

**Usage:**
```
WebScreen> /backup save production
[OK] Configuration backed up to /backups/production.json

WebScreen> /backup list
Available backups:
Name                     Size        Date
----------------------------------------
production               1.2 KB      45 sec ago
dev_config              1.1 KB      3600 sec ago
testing                 1.3 KB      7200 sec ago

WebScreen> /backup restore production
[OK] Configuration restored from production
Please reboot for changes to take effect
```

**Features:**
- **Auto Naming**: Generates timestamp-based names if not specified
- **Metadata Storage**: Saves timestamp, WiFi SSID, memory status
- **Directory Management**: Creates `/backups` directory automatically
- **Listing Support**: Shows all backups with age information

**Operations:**
- `save [name]` - Create a new backup
- `restore <name>` - Restore a specific backup
- `list` - Show all available backups

**Use Cases:**
- Save configuration before making changes
- Create environment-specific configurations
- Quick rollback to known-good settings
- A/B testing different configurations

#### `/monitor [cpu|mem|net|all]`
Provides real-time system monitoring with auto-refresh.

**Usage:**
```
WebScreen> /monitor mem
Live Monitor - Press any key to stop
=====================================
[14:23:45] Heap: 234.5KB/320.0KB (73.3%) | PSRAM: 7.2MB/8.0MB (90.0%)
```

**Monitor Modes:**

**Memory Mode (`mem` or `memory`):**
```
[HH:MM:SS] Heap: FREE/TOTAL (%) | PSRAM: FREE/TOTAL (%)
```
- Shows heap and PSRAM usage
- Displays percentages for quick assessment
- Updates every second

**CPU Mode (`cpu`):**
```
[HH:MM:SS] CPU: 240 MHz | Load: 45.2% | Temp: 42.3°C | Tasks: 12
```
- CPU frequency and utilization
- Core temperature monitoring
- FreeRTOS task count

**Network Mode (`net` or `network`):**
```
[HH:MM:SS] WiFi: MyNetwork | IP: 192.168.1.100 | RSSI: -45 dBm | Channel: 6
```
- Current WiFi connection
- IP address assignment
- Signal strength (RSSI)
- WiFi channel number

**All Mode (`all`):**
- Cycles through all metrics
- Shows different stat each second
- Comprehensive system overview

**Features:**
- **Real-time Updates**: Refreshes every second
- **Non-blocking**: Press any key to stop
- **Auto-stop**: Ends automatically after 30 seconds without input
- **Timestamped**: Each update shows current time
- **ANSI Formatting**: Clean single-line updates

**Use Cases:**
- Monitor memory during JavaScript execution
- Track CPU temperature under load
- Debug WiFi connectivity issues
- Performance profiling during development

### Time Commands

#### `/time`
Displays the current device time, epoch timestamp, and day of week. Time is sourced from NTP (Network Time Protocol) and requires a WiFi connection for initial synchronization.

**Usage:**
```
WebScreen> /time
Current time: 2026-02-17 14:23:45
Epoch: 1771337025
Day of week: 2 (0=Sun)
```

**Notes:**
- Returns an error if NTP has not synced yet
- Time is displayed in the device's configured timezone
- NTP automatically syncs when WiFi connects (server: `pool.ntp.org` by default)

#### `/settime <epoch> [timezone]`
Manually sets the device time from a Unix epoch timestamp, with an optional POSIX timezone string.

**Usage:**
```
WebScreen> /settime 1771337025
Time set: 2026-02-17 14:23:45

WebScreen> /settime 1771337025 EST5EDT,M3.2.0,M11.1.0
Time set: 2026-02-17 09:23:45
```

**Parameters:**
- `epoch` - Unix timestamp in seconds (must be after 2021-01-01)
- `timezone` - Optional POSIX TZ string that sets the device timezone

**POSIX TZ String Examples:**
| Location | POSIX TZ String |
|---|---|
| UTC | `UTC0` |
| US Eastern | `EST5EDT,M3.2.0,M11.1.0` |
| US Pacific | `PST8PDT,M3.2.0,M11.1.0` |
| Buenos Aires | `<-03>3` |
| Tokyo | `JST-9` |
| London | `GMT0BST,M3.5.0/1,M10.5.0` |
| Berlin | `CET-1CEST,M3.5.0,M10.5.0/3` |

**Notes:**
- The WebScreen Admin tool provides a timezone dropdown that maps IANA names to POSIX TZ strings automatically
- Full POSIX TZ database: https://github.com/nayarsystems/posix_tz_db

**Use Cases:**
- Set time when WiFi/NTP is not available
- Override timezone from the serial console
- Used by the WebScreen Admin tool's "Sync Time to Device" feature

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

#### `/ls [path] [json]` or `/list [path] [json]`
Lists files and directories on the SD card. A trailing `json` token switches to a single-line machine-readable listing for host tools.

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
Total: 4 files, 2 directories

WebScreen> /ls /apps
Directory listing for: /apps
Type    Size        Name
--------------------------------
FILE    3.4 KB      dashboard.js
FILE    1.8 KB      clock.js
FILE    2.1 KB      notifications.js
Total: 3 files, 0 directories
```

**JSON Mode:**
```
WebScreen> /ls /apps json
{"path":"/apps","entries":[{"name":"dashboard.js","dir":false,"size":3481},{"name":"clock.js","dir":false,"size":1843},{"name":"notifications.js","dir":false,"size":2150}]}
```
- Emitted as a single line — read until the newline and parse as JSON
- Each entry has `name` (string), `dir` (boolean), and `size` (bytes; always `0` for directories)
- `/ls json` (no path) lists the root directory

**Default Path:** If no path is specified, lists the root directory (`/`)

**Output Format (plain listing):**
- **Type**: `FILE` or `DIR`
- **Size**: File size in human-readable format (KB, MB, GB)
- **Name**: File or directory name
- **End Marker**: The listing always ends with a `Total: N files, M directories` line, so host tools know the listing is complete

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

#### `/rm <file|empty-dir>` or `/delete <file|empty-dir>`
Deletes a file — or an **empty** directory — from the SD card. Directories are removed with `rmdir`, so a directory must be emptied first.

**Usage:**
```
WebScreen> /rm old_script.js
[OK] File deleted: /old_script.js

WebScreen> /rm debug.log
[OK] File deleted: /debug.log

WebScreen> /rm old_assets
[OK] Directory removed: /old_assets

WebScreen> /rm apps
[ERROR] Cannot remove directory (not empty?): /apps
```

**Safety Features:**
- **Confirmation Messages**: Clear feedback on successful deletion
- **Error Reporting**: Helpful error messages if deletion fails
- **Path Normalization**: Handles paths with or without leading slashes
- **Empty Directories Only**: A non-empty directory is never deleted recursively — remove its contents first

**Warning:** File deletion is permanent. Ensure you have backups if needed.

#### `/mkdir <path>`
Creates a directory on the SD card.

**Usage:**
```
WebScreen> /mkdir /apps
[OK] Directory created: /apps

WebScreen> /mkdir /apps
[ERROR] Already exists: /apps
```

**Features:**
- **Path Normalization**: Handles paths with or without leading slashes
- **Existence Check**: Reports an error instead of silently succeeding if the path already exists

**Use Cases:**
- Organize scripts and assets into directories before `/upload`
- Create the target directory for `/wget` downloads

#### `/download <file>` or `/dl <file>`
Dumps any file from the SD card as base64 over serial, for binary-safe host-side download. This is the reverse of `/upload <file> base64`.

**Usage:**
```
WebScreen> /download webscreen.json
=== DOWNLOAD /webscreen.json SIZE 214 ===
eyJzZXR0aW5ncyI6eyJ3aWZpIjp7InNzaWQiOiJNeU5ldHdvcmsiLCJwYXNzIjoiTXlQYXNzd29y
ZCJ9fSwiZGlzcGxheSI6eyJicmlnaHRuZXNzIjoyMDB9LCJzY3JpcHQiOiJhcHAuanMifQ==
=== DOWNLOAD END ===
```

**Stream Format:**
- Header line: `=== DOWNLOAD <path> SIZE <n> ===` (`<path>` is the normalized full path, `<n>` is the file size in bytes)
- Body: base64-encoded file contents, 76 characters per line (57 raw bytes per line, classic MIME width)
- Footer line: `=== DOWNLOAD END ===`
- Concatenate the body lines and base64-decode them on the host; verify the result against the `SIZE` value from the header

**Notes:**
- Works for any file, text or binary (images, GIFs, fonts, ...)
- Do not confuse with `/wget` (formerly aliased `download`), which downloads from a URL **to** the device

**Use Cases:**
- Pull a script or configuration off the device without removing the SD card
- Retrieve logs or data files written by a JavaScript application
- Back up binary assets to the host

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
[OK] Loading script: /prototype.js
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

WebScreen> /mkdir /apps
[OK] Directory created: /apps

WebScreen> /write apps/weather.js
[... create organized application structure ...]

WebScreen> /write apps/clock.js
[... create another organized app ...]

WebScreen> /ls /apps
[... verify organization ...]

WebScreen> /rm /apps/clock.js
[OK] File deleted: /apps/clock.js

WebScreen> /rm /apps
[ERROR] Cannot remove directory (not empty?): /apps
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
- Commands are implemented in `serial_commands.cpp` and dispatched from both fallback and dynamic JS modes
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