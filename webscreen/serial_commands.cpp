#include "serial_commands.h"
#include "globals.h"
#include "webscreen_config.h"
#include "webscreen_hardware.h"
#include "webscreen_runtime.h"
#include "webscreen_base64.h"
#include "webscreen_serial_line.h"
#include <WiFi.h>
#include <time.h>
#include <sys/time.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Abort blocking receive loops if the host stops sending (loopTask must never hang)
static const unsigned long SERIAL_RX_TIMEOUT_MS = 30000;

enum ArgStyle : uint8_t {
  ARGS_IGNORED,
  ARGS_RAW,
  ARGS_DEFAULT_ROOT,
};

struct SerialCommands::Command {
  const char* name;
  const char* alias;
  void (*handler)(const String& args);
  ArgStyle argStyle;
  const char* usage;
  const char* desc;
};

// Row order = /help listing order; a nullptr handler marks a help-only row.
const SerialCommands::Command SerialCommands::kCommands[] = {
  { "help",        "h",        [](const String&) { showHelp(); },   ARGS_IGNORED,      "/help",                    "Show this help" },
  { "stats",       nullptr,    [](const String&) { showStats(); },  ARGS_IGNORED,      "/stats",                   "Show system statistics" },
  { "info",        nullptr,    [](const String&) { showInfo(); },   ARGS_IGNORED,      "/info",                    "Show device information" },
  { "write",       nullptr,    writeScript,                         ARGS_RAW,          "/write <filename>",        "Write JS script to SD card (interactive)" },
  { "upload",      nullptr,    uploadFile,                          ARGS_RAW,          "/upload <file> [base64]",  "Upload any file (text or base64-encoded)" },
  { "config",      nullptr,    configCommand,                       ARGS_RAW,          "/config get <key>",        "Get config value from webscreen.json" },
  { "config",      nullptr,    nullptr,                             ARGS_RAW,          "/config set <key> <val>",  "Set config value in webscreen.json" },
  { "ls",          "list",     listFiles,                           ARGS_DEFAULT_ROOT, "/ls [path] [json]",        "List files/directories (json = machine-readable)" },
  { "cat",         "view",     catFile,                             ARGS_RAW,          "/cat <file>",              "Display file contents" },
  { "rm",          "delete",   deleteFile,                          ARGS_RAW,          "/rm <file|empty-dir>",     "Delete file or empty directory" },
  { "mkdir",       nullptr,    makeDirectory,                       ARGS_RAW,          "/mkdir <path>",            "Create directory on SD card" },
  { "download",    "dl",       downloadFile64,                      ARGS_RAW,          "/download <file>",         "Dump file as base64 (host-side download)" },
  { "load",        "run",      loadApp,                             ARGS_RAW,          "/load <script.js> [save]", "Load/switch to different JS app (save = persist to config)" },
  { "restart_app", nullptr,    [](const String&) { restartApp(); }, ARGS_IGNORED,      "/restart_app",             "Restart the JS app in place (no reboot)" },
  { "eval",        nullptr,    evalJs,                              ARGS_RAW,          "/eval <js-code>",          "Evaluate JS in the running app (REPL)" },
  { "errors",      nullptr,    [](const String&) { showErrors(); }, ARGS_IGNORED,      "/errors",                  "Show last JS error and restart-ladder state" },
  { "gc",          nullptr,    [](const String&) { runGC(); },      ARGS_IGNORED,      "/gc",                      "Run JS garbage collection" },
  { "screenshot",  "ss",       [](const String&) { screenshot(); }, ARGS_IGNORED,      "/screenshot",              "Capture the screen as base64 RGB565" },
  { "wget",        "fetch",    wget,                                ARGS_RAW,          "/wget <url> [file]",       "Download file from URL to SD card" },
  { "ping",        nullptr,    ping,                                ARGS_RAW,          "/ping <host>",             "Test network connectivity" },
  { "backup",      nullptr,    backup,                              ARGS_RAW,          "/backup [save|restore]",   "Backup/restore configuration" },
  { "monitor",     "mon",      monitor,                             ARGS_RAW,          "/monitor [cpu|mem|net]",   "Live system monitoring" },
  { "brightness",  nullptr,    setBrightness,                       ARGS_RAW,          "/brightness <0-255>",      "Set display brightness" },
  { "time",        nullptr,    [](const String&) { showTime(); },   ARGS_IGNORED,      "/time",                    "Show current device time" },
  { "settime",     nullptr,    setTime,                             ARGS_RAW,          "/settime <epoch> [tz]",    "Set device time from epoch" },
  { "factory_reset", nullptr,  factoryReset,                        ARGS_RAW,          "/factory_reset confirm",   "Delete webscreen.json and reboot to fallback" },
  { "reboot",      "restart",  [](const String&) { reboot(); },     ARGS_IGNORED,      "/reboot",                  "Restart the device" },
};

const size_t SerialCommands::kCommandCount = sizeof(SerialCommands::kCommands) / sizeof(SerialCommands::kCommands[0]);

void SerialCommands::init() {
  Serial.println("\n=== WebScreen Serial Console ===");
  Serial.println("Type /help for available commands");
  printPrompt();
}

bool SerialCommands::readLine(String& line) {
  static WebscreenSerialLine<1024> input;
  // Bound each poll so continuous serial traffic cannot starve the power button.
  for (size_t i = 0; i < 256 && Serial.available(); i++) {
    int c = Serial.read();
    if (c < 0) break;
    auto result = input.push((char)c);
    if (result == WebscreenSerialLine<1024>::Overflow) {
      printError("Input line too long (maximum 1023 bytes)");
      return false;
    }
    if (result == WebscreenSerialLine<1024>::Ready) {
      line = input.data();
      return true;
    }
  }
  return false;
}

void SerialCommands::processCommand(const String& command) {
  String cmd = command;
  cmd.trim();
  
  if (cmd.length() == 0) {
    printPrompt();
    return;
  }
  
  if (!cmd.startsWith("/")) {
    printError("Commands must start with '/'. Type /help for help.");
    printPrompt();
    return;
  }
  
  // Parse command and arguments
  int spaceIndex = cmd.indexOf(' ');
  String baseCmd = (spaceIndex > 0) ? cmd.substring(1, spaceIndex) : cmd.substring(1);
  String args = (spaceIndex > 0) ? cmd.substring(spaceIndex + 1) : "";
  
  baseCmd.toLowerCase();

  const Command* match = nullptr;
  for (size_t i = 0; i < kCommandCount; i++) {
    const Command& c = kCommands[i];
    if (c.handler == nullptr) continue;
    if (baseCmd == c.name || (c.alias != nullptr && baseCmd == c.alias)) {
      match = &c;
      break;
    }
  }

  if (match == nullptr) {
    printError("Unknown command: " + baseCmd + ". Type /help for available commands.");
  } else if (match->argStyle == ARGS_DEFAULT_ROOT && args.length() == 0) {
    match->handler("/");
  } else {
    match->handler(args);
  }

  printPrompt();
}

void SerialCommands::configCommand(const String& args) {
  if (args.startsWith("get ")) {
    configGet(args.substring(4));
  } else if (args.startsWith("set ")) {
    configSet(args.substring(4));
  } else {
    printError("Usage: /config get <key> or /config set <key> <value>");
  }
}

void SerialCommands::showHelp() {
  Serial.println("\n=== WebScreen Commands ===");
  for (size_t i = 0; i < kCommandCount; i++) {
    const Command& c = kCommands[i];
    char line[96];
    snprintf(line, sizeof(line), "%-25s- %s", c.usage, c.desc);
    Serial.println(line);
  }
  Serial.println("\nExamples:");
  Serial.println("/write hello.js");
  Serial.println("/upload image.png base64");
  Serial.println("/upload config.json");
  Serial.println("/config get wifi.ssid");
  Serial.println("/config set wifi.ssid MyNetwork");
  Serial.println("/ls /");
  Serial.println("/cat webscreen.json");
}

void SerialCommands::showStats() {
  Serial.println("\n=== System Statistics ===");
  
  // Memory
  Serial.printf("Free Heap: %s\n", formatBytes(ESP.getFreeHeap()).c_str());
  Serial.printf("Total Heap: %s\n", formatBytes(ESP.getHeapSize()).c_str());
  Serial.printf("Free PSRAM: %s\n", formatBytes(ESP.getFreePsram()).c_str());
  Serial.printf("Total PSRAM: %s\n", formatBytes(ESP.getPsramSize()).c_str());
  Serial.printf("Heap Low Watermark: %s\n", formatBytes(esp_get_minimum_free_heap_size()).c_str());
  Serial.printf("Largest Free Block: %s\n",
                formatBytes(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)).c_str());

  // JavaScript engine
  uint32_t jsUsed = 0, jsTotal = 0;
  webscreen_runtime_get_js_arena(&jsUsed, &jsTotal);
  if (jsTotal > 0) {
    Serial.printf("JS Arena Used: %s\n", formatBytes(jsUsed).c_str());
    Serial.printf("JS Arena Total: %s\n", formatBytes(jsTotal).c_str());
  } else {
    Serial.println("JS Arena: Not running");
  }

  // Storage
  if (SD_MMC.cardSize() > 0) {
    uint64_t cardSize = SD_MMC.cardSize();
    uint64_t usedBytes = SD_MMC.usedBytes();
    Serial.printf("SD Card Size: %s\n", formatBytes(cardSize).c_str());
    Serial.printf("SD Card Used: %s\n", formatBytes(usedBytes).c_str());
    Serial.printf("SD Card Free: %s\n", formatBytes(cardSize - usedBytes).c_str());
  } else {
    Serial.println("SD Card: Not mounted");
  }
  
  // Network
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("WiFi: Connected to %s\n", WiFi.SSID().c_str());
    Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("Signal Strength: %d dBm\n", WiFi.RSSI());
  } else {
    Serial.println("WiFi: Disconnected");
  }
  
  // Uptime
  Serial.printf("Uptime: %lu seconds\n", millis() / 1000);
  Serial.printf("CPU Frequency: %d MHz\n", ESP.getCpuFreqMHz());
}

void SerialCommands::showInfo() {
  Serial.println("\n=== Device Information ===");
  Serial.printf("Chip Model: %s\n", ESP.getChipModel());
  Serial.printf("Chip Revision: %d\n", ESP.getChipRevision());
  Serial.printf("Flash Size: %s\n", formatBytes(ESP.getFlashChipSize()).c_str());
  Serial.printf("Flash Speed: %d MHz\n", ESP.getFlashChipSpeed() / 1000000);
  
  uint8_t mac[6];
  WiFi.macAddress(mac);
  Serial.printf("MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\n", 
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  
  Serial.printf("SDK Version: %s\n", ESP.getSdkVersion());
  Serial.println("WebScreen Version: " WEBSCREEN_VERSION_STRING);
  Serial.println("Build Date: " __DATE__ " " __TIME__);

  uint32_t jsUsed = 0, jsTotal = 0;
  webscreen_runtime_get_js_arena(&jsUsed, &jsTotal);
  if (jsTotal > 0) {
    Serial.printf("JS Arena Used: %s\n", formatBytes(jsUsed).c_str());
    Serial.printf("JS Arena Total: %s\n", formatBytes(jsTotal).c_str());
  }
}

void SerialCommands::writeScript(const String& args) {
  if (args.length() == 0) {
    printError("Usage: /write <filename>");
    return;
  }
  
  if (!sdReady()) {
    printError("SD card not available");
    return;
  }
  
  String filename = "/" + args;
  if (!filename.endsWith(".js")) {
    filename += ".js";
  }
  
  Serial.println("Enter JavaScript code. End with a line containing only 'END':");
  Serial.println("---");
  
  File file = SD_MMC.open(filename, FILE_WRITE);
  if (!file) {
    printError("Cannot create file: " + filename);
    return;
  }
  
  String line;
  while (true) {
    unsigned long waitStart = millis();
    while (!Serial.available()) {
      if (millis() - waitStart >= SERIAL_RX_TIMEOUT_MS) {
        file.close();
        SD_MMC.remove(filename);
        printError("Write aborted: no data received for 30 seconds (" + filename + " removed)");
        return;
      }
      delay(10);
    }

    line = Serial.readStringUntil('\n');
    line.trim();

    if (line == "END") {
      break;
    }

    file.println(line);
    Serial.println("+ " + line);
  }

  file.close();
  printSuccess("Script saved: " + filename + " (" + formatBytes(SD_MMC.open(filename).size()) + ")");
}

// Base64 decoding table
static const uint8_t base64_decode_table[128] = {
  64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
  64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
  64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
  52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
  64,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
  15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
  64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
  41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64
};

// Returns the decoded byte count, or -1 if the decoded data would overflow outputSize.
static int base64_decode(const char* input, size_t inputLen, uint8_t* output, size_t outputSize) {
  size_t outputLen = 0;
  uint32_t buffer = 0;
  int bitsCollected = 0;

  for (size_t i = 0; i < inputLen; i++) {
    char c = input[i];
    if (c == '=' || c == '\n' || c == '\r' || c == ' ') continue;
    if (c < 0 || c >= 128) continue;

    uint8_t value = base64_decode_table[(uint8_t)c];
    if (value >= 64) continue;

    buffer = (buffer << 6) | value;
    bitsCollected += 6;

    if (bitsCollected >= 8) {
      bitsCollected -= 8;
      if (outputLen >= outputSize) {
        return -1;
      }
      output[outputLen++] = (buffer >> bitsCollected) & 0xFF;
    }
  }

  return (int)outputLen;
}

void SerialCommands::uploadFile(const String& args) {
  if (args.length() == 0) {
    printError("Usage: /upload <filename> [base64]");
    return;
  }

  if (!sdReady()) {
    printError("SD card not available");
    return;
  }

  // Parse filename and mode
  int spaceIndex = args.indexOf(' ');
  String filename = (spaceIndex > 0) ? args.substring(0, spaceIndex) : args;
  String mode = (spaceIndex > 0) ? args.substring(spaceIndex + 1) : "";
  mode.toLowerCase();
  mode.trim();

  bool isBase64 = (mode == "base64" || mode == "b64");

  // Ensure filename starts with /
  if (!filename.startsWith("/")) {
    filename = "/" + filename;
  }

  Serial.println("Upload mode: " + String(isBase64 ? "base64" : "text"));
  Serial.println("Target file: " + filename);
  Serial.println("Send file data. End with a line containing only 'END':");
  Serial.println("---");

  File file = SD_MMC.open(filename, FILE_WRITE);
  if (!file) {
    printError("Cannot create file: " + filename);
    return;
  }

  size_t totalBytes = 0;
  String line;
  bool aborted = false;
  String abortReason;

  // Buffer for base64 decoding
  uint8_t decodeBuffer[512];

  while (true) {
    unsigned long waitStart = millis();
    bool timedOut = false;
    while (!Serial.available()) {
      if (millis() - waitStart >= SERIAL_RX_TIMEOUT_MS) {
        timedOut = true;
        break;
      }
      delay(10);
    }
    if (timedOut) {
      aborted = true;
      abortReason = "no data received for 30 seconds";
      break;
    }

    line = Serial.readStringUntil('\n');
    line.trim();

    if (line == "END") {
      break;
    }

    if (aborted) {
      continue;  // drain remaining chunks until END so the stream stays in sync
    }

    if (isBase64) {
      // Decode base64 and write binary data
      int decodedLen = base64_decode(line.c_str(), line.length(), decodeBuffer, sizeof(decodeBuffer));
      if (decodedLen < 0) {
        aborted = true;
        abortReason = "chunk exceeds " + String((unsigned int)sizeof(decodeBuffer)) + " decoded bytes per line";
        printError("Upload aborted: " + abortReason);
        continue;
      }
      if (decodedLen > 0) {
        file.write(decodeBuffer, (size_t)decodedLen);
        totalBytes += (size_t)decodedLen;
      }
      // Show progress every 10KB
      if (totalBytes % 10240 < 512) {
        Serial.printf("+ %s received\r", formatBytes(totalBytes).c_str());
      }
    } else {
      // Text mode - write as-is with newline
      file.println(line);
      totalBytes += line.length() + 1;
      Serial.println("+ " + line);
    }
  }

  file.close();
  Serial.println();
  if (aborted) {
    SD_MMC.remove(filename);  // partial file is unusable
    printError("Upload failed: " + abortReason + " (" + filename + " removed)");
  } else {
    printSuccess("File saved: " + filename + " (" + formatBytes(totalBytes) + ")");
  }
}

void SerialCommands::configSet(const String& args) {
  int spaceIndex = args.indexOf(' ');
  if (spaceIndex < 0) {
    printError("Usage: /config set <key> <value>");
    return;
  }

  String key = args.substring(0, spaceIndex);
  String value = args.substring(spaceIndex + 1);

  if (!sdReady()) {
    printError("SD card not available");
    return;
  }
  
  // Read existing config
  DynamicJsonDocument doc(2048);
  File file = SD_MMC.open("/webscreen.json", FILE_READ);
  
  if (file) {
    deserializeJson(doc, file);
    file.close();
  }
  
  // Set nested key (e.g., "wifi.ssid")
  if (key.indexOf('.') > 0) {
    String section = key.substring(0, key.indexOf('.'));
    String subkey = key.substring(key.indexOf('.') + 1);
    doc[section][subkey] = value;
  } else {
    doc[key] = value;
  }
  
  // Write back to file
  file = SD_MMC.open("/webscreen.json", FILE_WRITE);
  if (!file) {
    printError("Cannot write to webscreen.json");
    return;
  }
  
  serializeJsonPretty(doc, file);
  file.close();
  
  printSuccess("Config updated: " + key + " = " + value);
}

void SerialCommands::configGet(const String& args) {
  String key = args;
  key.trim();
  
  if (key.length() == 0) {
    printError("Usage: /config get <key>");
    return;
  }
  
  if (!sdReady()) {
    printError("SD card not available");
    return;
  }
  
  File file = SD_MMC.open("/webscreen.json", FILE_READ);
  if (!file) {
    printError("Cannot read webscreen.json");
    return;
  }
  
  DynamicJsonDocument doc(2048);
  deserializeJson(doc, file);
  file.close();
  
  JsonVariant result = doc.as<JsonVariant>();

  // Handle nested keys (e.g., "settings.wifi.ssid")
  // Split by '.' and traverse the JSON tree
  String remaining = key;
  while (remaining.length() > 0 && !result.isNull()) {
    int dotPos = remaining.indexOf('.');
    String part;
    if (dotPos > 0) {
      part = remaining.substring(0, dotPos);
      remaining = remaining.substring(dotPos + 1);
    } else {
      part = remaining;
      remaining = "";
    }
    result = result[part];
  }
  
  if (result.isNull()) {
    printError("Key not found: " + key);
  } else {
    Serial.printf("%s = %s\n", key.c_str(), result.as<String>().c_str());
  }
}

// Print a string as a JSON value, escaping quotes/backslashes/control chars
static void printJsonString(const char* s) {
  Serial.print('"');
  for (; *s; s++) {
    char c = *s;
    if (c == '"' || c == '\\') {
      Serial.print('\\');
      Serial.print(c);
    } else if ((uint8_t)c < 0x20) {
      Serial.printf("\\u%04x", (unsigned)c);
    } else {
      Serial.print(c);
    }
  }
  Serial.print('"');
}

void SerialCommands::listFiles(const String& path) {
  if (!sdReady()) {
    printError("SD card not available");
    return;
  }

  // Trailing "json" token switches to a one-line machine-readable listing
  String p = path;
  p.trim();
  bool json = false;
  if (p == "json") {
    json = true;
    p = "/";
  } else if (p.endsWith(" json")) {
    json = true;
    p = p.substring(0, p.length() - 5);
    p.trim();
  }
  if (p.length() == 0) p = "/";

  File root = SD_MMC.open(p);
  if (!root || !root.isDirectory()) {
    printError("Cannot open directory: " + p);
    return;
  }

  size_t fileCount = 0, dirCount = 0;

  if (json) {
    Serial.print("{\"path\":");
    printJsonString(p.c_str());
    Serial.print(",\"entries\":[");
    File file = root.openNextFile();
    bool first = true;
    while (file) {
      if (!first) Serial.print(',');
      first = false;
      Serial.print("{\"name\":");
      printJsonString(file.name());
      Serial.printf(",\"dir\":%s,\"size\":%u}",
                    file.isDirectory() ? "true" : "false",
                    file.isDirectory() ? 0 : (unsigned)file.size());
      if (file.isDirectory()) dirCount++; else fileCount++;
      file = root.openNextFile();
    }
    Serial.println("]}");
  } else {
    Serial.println("\nDirectory listing for: " + p);
    Serial.println("Type    Size        Name");
    Serial.println("--------------------------------");

    File file = root.openNextFile();
    while (file) {
      Serial.printf("%-7s %-10s %s\n",
                    file.isDirectory() ? "DIR" : "FILE",
                    file.isDirectory() ? "" : formatBytes(file.size()).c_str(),
                    file.name());
      if (file.isDirectory()) dirCount++; else fileCount++;
      file = root.openNextFile();
    }
    // End marker so host tools know the listing is complete
    Serial.printf("Total: %u files, %u directories\n",
                  (unsigned)fileCount, (unsigned)dirCount);
  }

  root.close();
}

void SerialCommands::deleteFile(const String& path) {
  if (path.length() == 0) {
    printError("Usage: /rm <filename>");
    return;
  }
  
  if (!sdReady()) {
    printError("SD card not available");
    return;
  }
  
  String fullPath = path.startsWith("/") ? path : ("/" + path);

  File f = SD_MMC.open(fullPath);
  bool isDir = f && f.isDirectory();
  if (f) f.close();

  if (isDir) {
    if (SD_MMC.rmdir(fullPath)) {
      printSuccess("Directory removed: " + fullPath);
    } else {
      printError("Cannot remove directory (not empty?): " + fullPath);
    }
  } else if (SD_MMC.remove(fullPath)) {
    printSuccess("File deleted: " + fullPath);
  } else {
    printError("Cannot delete file: " + fullPath);
  }
}

void SerialCommands::makeDirectory(const String& path) {
  if (path.length() == 0) {
    printError("Usage: /mkdir <path>");
    return;
  }

  if (!sdReady()) {
    printError("SD card not available");
    return;
  }

  String fullPath = path.startsWith("/") ? path : ("/" + path);
  fullPath.trim();

  if (SD_MMC.exists(fullPath)) {
    printError("Already exists: " + fullPath);
  } else if (SD_MMC.mkdir(fullPath)) {
    printSuccess("Directory created: " + fullPath);
  } else {
    printError("Cannot create directory: " + fullPath);
  }
}

void SerialCommands::downloadFile64(const String& path) {
  if (path.length() == 0) {
    printError("Usage: /download <file>");
    return;
  }

  if (!sdReady()) {
    printError("SD card not available");
    return;
  }

  String fullPath = path.startsWith("/") ? path : ("/" + path);
  fullPath.trim();

  File file = SD_MMC.open(fullPath, FILE_READ);
  if (!file || file.isDirectory()) {
    if (file) file.close();
    printError("Cannot open file: " + fullPath);
    return;
  }

  Serial.printf("=== DOWNLOAD %s SIZE %u ===\n", fullPath.c_str(), (unsigned)file.size());

  // 57 raw bytes -> one 76-char base64 line (classic MIME width)
  uint8_t raw[57 * 8];
  char b64[80];
  size_t n;
  while ((n = file.read(raw, sizeof(raw))) > 0) {
    for (size_t off = 0; off < n; off += 57) {
      size_t chunk = n - off;
      if (chunk > 57) chunk = 57;
      webscreen_base64_encode(raw + off, chunk, b64);
      Serial.println(b64);
    }
  }
  file.close();

  Serial.println("=== DOWNLOAD END ===");
}

void SerialCommands::factoryReset(const String& args) {
  String a = args;
  a.trim();
  a.toLowerCase();
  if (a != "confirm") {
    printError("This deletes /webscreen.json and reboots into fallback mode. Run '/factory_reset confirm' to proceed.");
    return;
  }

  if (!sdReady()) {
    printError("SD card not available");
    return;
  }

  if (SD_MMC.exists("/webscreen.json") && !SD_MMC.remove("/webscreen.json")) {
    printError("Cannot delete /webscreen.json");
    return;
  }

  printSuccess("Configuration deleted. Rebooting into fallback mode in 3 seconds...");
  delay(3000);
  ESP.restart();
}

void SerialCommands::screenshot() {
  if (webscreen_runtime_request_screenshot()) {
    Serial.println("Queued. Data follows as an '=== SCREENSHOT ... ===' block");
  } else {
    printError("Screenshot unavailable (JS runtime not running, or a capture is in flight)");
  }
}

void SerialCommands::catFile(const String& path) {
  if (path.length() == 0) {
    printError("Usage: /cat <filename>");
    return;
  }
  
  if (!sdReady()) {
    printError("SD card not available");
    return;
  }
  
  String fullPath = path.startsWith("/") ? path : ("/" + path);
  
  File file = SD_MMC.open(fullPath, FILE_READ);
  if (!file) {
    printError("Cannot open file: " + fullPath);
    return;
  }
  
  Serial.println("\n--- " + fullPath + " ---");
  while (file.available()) {
    Serial.write(file.read());
  }
  file.close();
  Serial.println("\n--- End of file ---");
}

void SerialCommands::reboot() {
  printSuccess("Rebooting in 3 seconds...");
  delay(3000);
  ESP.restart();
}

void SerialCommands::loadApp(const String& scriptName) {
  if (scriptName.length() == 0) {
    printError("Usage: /load <script.js> [save]");
    return;
  }

  if (!webscreen_runtime_is_javascript_active()) {
    printError("JS runtime is not running (fallback mode). Set the script in webscreen.json (/config set script <file>) and /reboot.");
    return;
  }

  if (!sdReady()) {
    printError("SD card not available");
    return;
  }

  // Trailing "save" also persists the script to webscreen.json (otherwise /load is session-only)
  String name = scriptName;
  bool persist = false;
  int spaceIndex = name.indexOf(' ');
  if (spaceIndex > 0) {
    String opt = name.substring(spaceIndex + 1);
    opt.trim();
    opt.toLowerCase();
    if (opt == "save") {
      persist = true;
      name = name.substring(0, spaceIndex);
    }
  }

  String fullPath = name.startsWith("/") ? name : ("/" + name);
  if (!fullPath.endsWith(".js")) {
    fullPath += ".js";
  }

  // Check if file exists
  File file = SD_MMC.open(fullPath, FILE_READ);
  if (!file) {
    printError("Script not found: " + fullPath);
    return;
  }
  file.close();

  if (webscreen_runtime_load_new_script(fullPath.c_str())) {
    printSuccess("Loading script: " + fullPath);
    if (persist) {
      configSet("script " + fullPath);
    }
  } else {
    printError("Cannot load script: " + fullPath);
  }
}

// Re-running SD_MMC.begin() per command re-probes the card; only remount when the card is absent.
bool SerialCommands::sdReady() {
  if (SD_MMC.cardType() != CARD_NONE) {
    return true;
  }
  return webscreen_hardware_init_sd_card();
}

void SerialCommands::printPrompt() {
  Serial.print("\nWebScreen> ");
}

String SerialCommands::formatBytes(size_t bytes) {
  if (bytes < 1024) return String(bytes) + " B";
  else if (bytes < 1024 * 1024) return String(bytes / 1024.0, 1) + " KB";
  else if (bytes < 1024 * 1024 * 1024) return String(bytes / (1024.0 * 1024.0), 1) + " MB";
  else return String(bytes / (1024.0 * 1024.0 * 1024.0), 1) + " GB";
}

void SerialCommands::printError(const String& message) {
  Serial.println("[ERROR] " + message);
}

void SerialCommands::printSuccess(const String& message) {
  Serial.println("[OK] " + message);
}

void SerialCommands::wget(const String& args) {
  if (args.length() == 0) {
    printError("Usage: /wget <url> [filename]");
    return;
  }
  
  // Parse URL and optional filename
  int spaceIndex = args.indexOf(' ');
  String url = (spaceIndex > 0) ? args.substring(0, spaceIndex) : args;
  String filename = "";
  
  if (spaceIndex > 0) {
    filename = args.substring(spaceIndex + 1);
  } else {
    // Extract filename from URL
    int lastSlash = url.lastIndexOf('/');
    if (lastSlash >= 0 && lastSlash < url.length() - 1) {
      filename = url.substring(lastSlash + 1);
    } else {
      filename = "download.dat";
    }
  }
  
  // Ensure filename starts with /
  if (!filename.startsWith("/")) {
    filename = "/" + filename;
  }
  
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    printError("WiFi not connected. Cannot download.");
    return;
  }
  
  // Check SD card
  if (!sdReady()) {
    printError("SD card not available");
    return;
  }
  
  Serial.println("Downloading: " + url);
  Serial.println("Saving to: " + filename);
  
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure(); // For simplicity, skip certificate validation
  
  // Start HTTP request
  if (url.startsWith("https://")) {
    http.begin(client, url);
  } else {
    http.begin(url);
  }
  
  http.setTimeout(30000); // 30 second timeout
  
  int httpCode = http.GET();
  
  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_OK) {
      // Get content length
      int contentLength = http.getSize();
      Serial.printf("Content-Length: %s\n", contentLength > 0 ? formatBytes(contentLength).c_str() : "Unknown");
      
      // Open file for writing
      File file = SD_MMC.open(filename, FILE_WRITE);
      if (!file) {
        printError("Cannot create file: " + filename);
        http.end();
        return;
      }
      
      // Get stream
      WiFiClient* stream = http.getStreamPtr();
      
      // Buffer for reading
      uint8_t buffer[512];
      int totalBytes = 0;
      int lastProgress = -1;
      
      // Download with progress
      Serial.print("Progress: ");
      while (http.connected() && (contentLength < 0 || totalBytes < contentLength)) {
        size_t available = stream->available();
        if (available) {
          int bytesRead = stream->readBytes(buffer, min(available, sizeof(buffer)));
          file.write(buffer, bytesRead);
          totalBytes += bytesRead;
          
          // Show progress
          if (contentLength > 0) {
            int progress = (totalBytes * 100) / contentLength;
            if (progress != lastProgress && progress % 10 == 0) {
              Serial.printf("%d%% ", progress);
              lastProgress = progress;
            }
          } else {
            // Show bytes downloaded if content length unknown
            if (totalBytes % 10240 == 0) { // Every 10KB
              Serial.print(".");
            }
          }
        }
        delay(1);
      }
      
      file.close();
      Serial.println();
      
      printSuccess("Downloaded " + formatBytes(totalBytes) + " to " + filename);
    } else {
      printError("HTTP error code: " + String(httpCode));
    }
  } else {
    printError("Connection failed: " + http.errorToString(httpCode));
  }
  
  http.end();
}

void SerialCommands::ping(const String& args) {
  if (args.length() == 0) {
    printError("Usage: /ping <host>");
    return;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    printError("WiFi not connected");
    return;
  }
  
  String host = args;
  host.trim();
  
  Serial.println("PING " + host);
  
  // Resolve hostname to IP
  IPAddress ip;
  if (!WiFi.hostByName(host.c_str(), ip)) {
    printError("Cannot resolve host: " + host);
    return;
  }
  
  Serial.printf("Pinging %s (%s) with 32 bytes of data:\n", host.c_str(), ip.toString().c_str());
  
  // Perform 4 pings
  int successCount = 0;
  int totalTime = 0;
  int minTime = 9999;
  int maxTime = 0;
  
  for (int i = 0; i < 4; i++) {
    unsigned long startTime = millis();
    
    // Simple ping implementation using TCP connect
    WiFiClient client;
    client.setTimeout(1000); // 1 second timeout
    
    bool success = false;
    int responseTime = 0;
    
    // Try to connect to port 80 (HTTP) as a connectivity test
    if (client.connect(ip, 80, 1000)) {
      responseTime = millis() - startTime;
      success = true;
      client.stop();
    } else {
      // Try port 443 (HTTPS) as fallback
      if (client.connect(ip, 443, 1000)) {
        responseTime = millis() - startTime;
        success = true;
        client.stop();
      }
    }
    
    if (success) {
      Serial.printf("Reply from %s: time=%dms\n", ip.toString().c_str(), responseTime);
      successCount++;
      totalTime += responseTime;
      if (responseTime < minTime) minTime = responseTime;
      if (responseTime > maxTime) maxTime = responseTime;
    } else {
      Serial.printf("Request timeout.\n");
    }
    
    if (i < 3) delay(1000); // Wait 1 second between pings
  }
  
  // Print statistics
  Serial.printf("\nPing statistics for %s:\n", ip.toString().c_str());
  Serial.printf("    Packets: Sent = 4, Received = %d, Lost = %d (%d%% loss)\n", 
                successCount, 4 - successCount, (4 - successCount) * 25);
  
  if (successCount > 0) {
    Serial.println("Approximate round trip times:");
    Serial.printf("    Minimum = %dms, Maximum = %dms, Average = %dms\n", 
                  minTime, maxTime, totalTime / successCount);
  }
}

void SerialCommands::backup(const String& args) {
  if (!sdReady()) {
    printError("SD card not available");
    return;
  }
  
  String operation = "";
  String backupName = "";
  
  // Parse arguments
  int spaceIndex = args.indexOf(' ');
  if (spaceIndex > 0) {
    operation = args.substring(0, spaceIndex);
    backupName = args.substring(spaceIndex + 1);
  } else {
    operation = args;
  }
  
  operation.toLowerCase();
  
  if (operation == "save") {
    // Generate backup name if not provided
    if (backupName.length() == 0) {
      backupName = String("backup_") + String(millis() / 1000);
    }
    
    // Create backups directory if it doesn't exist
    if (!SD_MMC.exists("/backups")) {
      SD_MMC.mkdir("/backups");
    }
    
    String backupPath = "/backups/" + backupName + ".json";
    
    // Read current configuration
    File srcFile = SD_MMC.open("/webscreen.json", FILE_READ);
    if (!srcFile) {
      printError("Cannot read webscreen.json");
      return;
    }
    
    // Write backup
    File dstFile = SD_MMC.open(backupPath, FILE_WRITE);
    if (!dstFile) {
      printError("Cannot create backup file");
      srcFile.close();
      return;
    }
    
    // Copy configuration
    while (srcFile.available()) {
      dstFile.write(srcFile.read());
    }
    
    srcFile.close();
    dstFile.close();
    
    // Add metadata file
    String metaPath = "/backups/" + backupName + ".meta";
    File metaFile = SD_MMC.open(metaPath, FILE_WRITE);
    if (metaFile) {
      metaFile.printf("{\n");
      metaFile.printf("  \"timestamp\": %lu,\n", millis() / 1000);
      metaFile.printf("  \"wifi_ssid\": \"%s\",\n", WiFi.SSID().c_str());
      metaFile.printf("  \"free_heap\": %d,\n", ESP.getFreeHeap());
      metaFile.printf("  \"version\": \"%s\"\n", WEBSCREEN_VERSION_STRING);
      metaFile.printf("}\n");
      metaFile.close();
    }
    
    printSuccess("Configuration backed up to " + backupPath);
    
  } else if (operation == "restore") {
    if (backupName.length() == 0) {
      printError("Usage: /backup restore <name>");
      return;
    }
    
    String backupPath = "/backups/" + backupName + ".json";
    
    // Check if backup exists
    if (!SD_MMC.exists(backupPath)) {
      printError("Backup not found: " + backupName);
      return;
    }
    
    // Read backup
    File backupFile = SD_MMC.open(backupPath, FILE_READ);
    if (!backupFile) {
      printError("Cannot read backup file");
      return;
    }
    
    // Write to main config
    File configFile = SD_MMC.open("/webscreen.json", FILE_WRITE);
    if (!configFile) {
      printError("Cannot write to webscreen.json");
      backupFile.close();
      return;
    }
    
    // Copy backup to config
    while (backupFile.available()) {
      configFile.write(backupFile.read());
    }
    
    backupFile.close();
    configFile.close();
    
    printSuccess("Configuration restored from " + backupName);
    Serial.println("Please reboot for changes to take effect");
    
  } else if (operation == "list" || operation == "") {
    // List available backups
    File backupsDir = SD_MMC.open("/backups");
    if (!backupsDir || !backupsDir.isDirectory()) {
      Serial.println("No backups found");
      return;
    }
    
    Serial.println("\nAvailable backups:");
    Serial.println("Name                     Size        Date");
    Serial.println("----------------------------------------");
    
    File file = backupsDir.openNextFile();
    while (file) {
      String name = String(file.name());
      if (name.endsWith(".json")) {
        name = name.substring(name.lastIndexOf('/') + 1);
        name = name.substring(0, name.length() - 5); // Remove .json
        
        // Try to read metadata
        String metaPath = String(file.name());
        metaPath.replace(".json", ".meta");
        File metaFile = SD_MMC.open(metaPath, FILE_READ);
        
        if (metaFile) {
          DynamicJsonDocument meta(256);
          deserializeJson(meta, metaFile);
          metaFile.close();
          
          unsigned long timestamp = meta["timestamp"] | 0;
          Serial.printf("%-24s %-10s %lu sec ago\n", 
                       name.c_str(), 
                       formatBytes(file.size()).c_str(),
                       (millis() / 1000) - timestamp);
        } else {
          Serial.printf("%-24s %-10s\n", name.c_str(), formatBytes(file.size()).c_str());
        }
      }
      file = backupsDir.openNextFile();
    }
    
    backupsDir.close();
    
  } else {
    printError("Usage: /backup [save|restore|list] [name]");
  }
}

void SerialCommands::monitor(const String& args) {
  String mode = args;
  mode.toLowerCase();
  mode.trim();
  
  if (mode == "") mode = "mem"; // Default to memory monitoring
  
  Serial.println("Live Monitor - Press any key to stop");
  Serial.println("=====================================");
  
  unsigned long lastUpdate = 0;
  const unsigned long updateInterval = 1000; // Update every second
  unsigned long monitorStart = millis();

  while (!Serial.available()) {
    if (millis() - monitorStart >= SERIAL_RX_TIMEOUT_MS) {
      Serial.println();
      printError("Monitor stopped: no input for 30 seconds");
      break;
    }
    if (millis() - lastUpdate >= updateInterval) {
      lastUpdate = millis();
      
      // Clear previous line (ANSI escape code)
      Serial.print("\r\033[K");
      
      if (mode == "mem" || mode == "memory") {
        // Memory monitoring
        Serial.printf("[%02d:%02d:%02d] Heap: %s/%s (%.1f%%) | PSRAM: %s/%s (%.1f%%)",
                     (int)((millis() / 3600000) % 24),
                     (int)((millis() / 60000) % 60),
                     (int)((millis() / 1000) % 60),
                     formatBytes(ESP.getFreeHeap()).c_str(),
                     formatBytes(ESP.getHeapSize()).c_str(),
                     (ESP.getFreeHeap() * 100.0) / ESP.getHeapSize(),
                     formatBytes(ESP.getFreePsram()).c_str(),
                     formatBytes(ESP.getPsramSize()).c_str(),
                     (ESP.getFreePsram() * 100.0) / ESP.getPsramSize());
                     
      } else if (mode == "cpu") {
        // CPU monitoring
        static unsigned long lastCycles = 0;
        unsigned long cycles = ESP.getCycleCount();
        unsigned long cyclesDiff = cycles - lastCycles;
        lastCycles = cycles;
        
        float cpuUsage = (cyclesDiff / (float)(ESP.getCpuFreqMHz() * 1000000)) * 100.0;
        
        Serial.printf("[%02d:%02d:%02d] CPU: %d MHz | Load: %.1f%% | Temp: %.1f°C | Tasks: %d",
                     (int)((millis() / 3600000) % 24),
                     (int)((millis() / 60000) % 60),
                     (int)((millis() / 1000) % 60),
                     ESP.getCpuFreqMHz(),
                     min(cpuUsage, 100.0f),
                     temperatureRead(),
                     uxTaskGetNumberOfTasks());
                     
      } else if (mode == "net" || mode == "network") {
        // Network monitoring
        if (WiFi.status() == WL_CONNECTED) {
          Serial.printf("[%02d:%02d:%02d] WiFi: %s | IP: %s | RSSI: %d dBm | Channel: %d",
                       (int)((millis() / 3600000) % 24),
                       (int)((millis() / 60000) % 60),
                       (int)((millis() / 1000) % 60),
                       WiFi.SSID().c_str(),
                       WiFi.localIP().toString().c_str(),
                       WiFi.RSSI(),
                       WiFi.channel());
        } else {
          Serial.printf("[%02d:%02d:%02d] WiFi: Disconnected",
                       (int)((millis() / 3600000) % 24),
                       (int)((millis() / 60000) % 60),
                       (int)((millis() / 1000) % 60));
        }
        
      } else if (mode == "all") {
        // Combined monitoring - cycle through different stats
        static int cycle = 0;
        
        switch (cycle % 3) {
          case 0:
            Serial.printf("[MEM] Heap: %s free | PSRAM: %s free",
                         formatBytes(ESP.getFreeHeap()).c_str(),
                         formatBytes(ESP.getFreePsram()).c_str());
            break;
          case 1:
            Serial.printf("[CPU] %d MHz | Temp: %.1f°C",
                         ESP.getCpuFreqMHz(),
                         temperatureRead());
            break;
          case 2:
            if (WiFi.status() == WL_CONNECTED) {
              Serial.printf("[NET] %s | RSSI: %d dBm",
                           WiFi.SSID().c_str(),
                           WiFi.RSSI());
            } else {
              Serial.printf("[NET] Disconnected");
            }
            break;
        }
        cycle++;
        
      } else {
        printError("Unknown monitor mode. Use: mem, cpu, net, or all");
        break;
      }
    }
    
    delay(100); // Small delay to prevent overwhelming the CPU
  }
  
  // Clear any pending serial input
  while (Serial.available()) {
    Serial.read();
  }
  
  Serial.println("\n\nMonitoring stopped.");
}

void SerialCommands::setBrightness(const String& args) {
  String val = args;
  val.trim();

  if (val.length() == 0) {
    Serial.printf("Current brightness: %d\n", webscreen_display_get_brightness());
    return;
  }

  int brightness = val.toInt();
  if (brightness < 0 || brightness > 255) {
    printError("Brightness must be between 0 and 255");
    return;
  }

  webscreen_display_set_brightness((uint8_t)brightness);
  printSuccess("Brightness set to " + String(brightness));
}

void SerialCommands::showTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 100)) {
    printError("Time not available (NTP not synced)");
    return;
  }

  Serial.printf("Current time: %04d-%02d-%02d %02d:%02d:%02d\n",
                timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  Serial.printf("Epoch: %lu\n", (unsigned long)mktime(&timeinfo));
  Serial.printf("Day of week: %d (0=Sun)\n", timeinfo.tm_wday);
}

void SerialCommands::setTime(const String& args) {
  String val = args;
  val.trim();

  if (val.length() == 0) {
    printError("Usage: /settime <epoch> [timezone]");
    return;
  }

  int spaceIndex = val.indexOf(' ');
  String epochStr = (spaceIndex > 0) ? val.substring(0, spaceIndex) : val;
  String tz = (spaceIndex > 0) ? val.substring(spaceIndex + 1) : "";
  tz.trim();

  unsigned long epoch = strtoul(epochStr.c_str(), NULL, 10);
  if (epoch < 1609459200) {  // Before 2021-01-01
    printError("Invalid epoch value");
    return;
  }

  struct timeval tv;
  tv.tv_sec = epoch;
  tv.tv_usec = 0;
  settimeofday(&tv, NULL);

  if (tz.length() > 0) {
    setenv("TZ", tz.c_str(), 1);
    tzset();
  }

  struct tm timeinfo;
  getLocalTime(&timeinfo);

  Serial.printf("Time set: %04d-%02d-%02d %02d:%02d:%02d\n",
                timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  printSuccess("Device time synchronized");
}

void SerialCommands::restartApp() {
  if (!webscreen_runtime_is_javascript_active()) {
    printError("JS runtime is not running (fallback mode). Use /reboot.");
    return;
  }
  webscreen_runtime_request_restart("serial /restart_app");
  printSuccess("JS app restart requested (in-place, no reboot)");
}

void SerialCommands::evalJs(const String& args) {
  String code = args;
  code.trim();
  if (code.length() == 0) {
    printError("Usage: /eval <js-code>   e.g. /eval print(mem_info())");
    return;
  }
  if (!webscreen_runtime_is_javascript_active()) {
    printError("JS runtime is not running (fallback mode)");
    return;
  }
  if (webscreen_runtime_eval_snippet(code.c_str())) {
    Serial.println("Queued. Result follows as [EVAL] ...");
  } else {
    printError("Cannot queue eval (busy, safe mode, or snippet longer than 255 chars)");
  }
}

void SerialCommands::showErrors() {
  webscreen_runtime_print_error_report();
}

void SerialCommands::runGC() {
  if (webscreen_runtime_garbage_collect()) {
    // GC runs on the JS task at its next safe point, so these numbers are pre-GC.
    uint32_t jsUsed = 0, jsTotal = 0;
    webscreen_runtime_get_js_arena(&jsUsed, &jsTotal);
    printSuccess("GC requested (runs at the JS task's next safe point). JS arena now: " + formatBytes(jsUsed) + " / " + formatBytes(jsTotal) + " used");
  } else {
    printError("Garbage collection unavailable (JS engine not running)");
  }
}
