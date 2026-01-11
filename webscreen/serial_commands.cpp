#include "serial_commands.h"
#include "globals.h"
#include <WiFi.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

void SerialCommands::init() {
  Serial.println("\n=== WebScreen Serial Console ===");
  Serial.println("Type /help for available commands");
  printPrompt();
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
  
  if (baseCmd == "help" || baseCmd == "h") {
    showHelp();
  }
  else if (baseCmd == "stats") {
    showStats();
  }
  else if (baseCmd == "info") {
    showInfo();
  }
  else if (baseCmd == "write") {
    writeScript(args);
  }
  else if (baseCmd == "upload") {
    uploadFile(args);
  }
  else if (baseCmd == "config") {
    if (args.startsWith("get ")) {
      configGet(args.substring(4));
    } else if (args.startsWith("set ")) {
      configSet(args.substring(4));
    } else {
      printError("Usage: /config get <key> or /config set <key> <value>");
    }
  }
  else if (baseCmd == "ls" || baseCmd == "list") {
    listFiles(args.length() > 0 ? args : "/");
  }
  else if (baseCmd == "rm" || baseCmd == "delete") {
    deleteFile(args);
  }
  else if (baseCmd == "cat" || baseCmd == "view") {
    catFile(args);
  }
  else if (baseCmd == "reboot" || baseCmd == "restart") {
    reboot();
  }
  else if (baseCmd == "load" || baseCmd == "run") {
    loadApp(args);
  }
  else if (baseCmd == "wget" || baseCmd == "download") {
    wget(args);
  }
  else if (baseCmd == "ping") {
    ping(args);
  }
  else if (baseCmd == "backup") {
    backup(args);
  }
  else if (baseCmd == "monitor" || baseCmd == "mon") {
    monitor(args);
  }
  else {
    printError("Unknown command: " + baseCmd + ". Type /help for available commands.");
  }
  
  printPrompt();
}

void SerialCommands::showHelp() {
  Serial.println("\n=== WebScreen Commands ===");
  Serial.println("/help                    - Show this help");
  Serial.println("/stats                   - Show system statistics");
  Serial.println("/info                    - Show device information");
  Serial.println("/write <filename>        - Write JS script to SD card (interactive)");
  Serial.println("/upload <file> [base64]  - Upload any file (text or base64-encoded)");
  Serial.println("/config get <key>        - Get config value from webscreen.json");
  Serial.println("/config set <key> <val>  - Set config value in webscreen.json");
  Serial.println("/ls [path]               - List files/directories");
  Serial.println("/cat <file>              - Display file contents");
  Serial.println("/rm <file>               - Delete file");
  Serial.println("/load <script.js>        - Load/switch to different JS app");
  Serial.println("/wget <url> [file]       - Download file from URL to SD card");
  Serial.println("/ping <host>             - Test network connectivity");
  Serial.println("/backup [save|restore]   - Backup/restore configuration");
  Serial.println("/monitor [cpu|mem|net]   - Live system monitoring");
  Serial.println("/reboot                  - Restart the device");
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
  Serial.println("WebScreen Version: 2.0.0");
  Serial.println("Build Date: " __DATE__ " " __TIME__);
}

void SerialCommands::writeScript(const String& args) {
  if (args.length() == 0) {
    printError("Usage: /write <filename>");
    return;
  }
  
  if (!SD_MMC.begin()) {
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
    while (!Serial.available()) {
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

static size_t base64_decode(const char* input, size_t inputLen, uint8_t* output) {
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
      output[outputLen++] = (buffer >> bitsCollected) & 0xFF;
    }
  }

  return outputLen;
}

void SerialCommands::uploadFile(const String& args) {
  if (args.length() == 0) {
    printError("Usage: /upload <filename> [base64]");
    return;
  }

  if (!SD_MMC.begin()) {
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

  // Buffer for base64 decoding
  uint8_t decodeBuffer[512];

  while (true) {
    while (!Serial.available()) {
      delay(10);
    }

    line = Serial.readStringUntil('\n');
    line.trim();

    if (line == "END") {
      break;
    }

    if (isBase64) {
      // Decode base64 and write binary data
      size_t decodedLen = base64_decode(line.c_str(), line.length(), decodeBuffer);
      if (decodedLen > 0) {
        file.write(decodeBuffer, decodedLen);
        totalBytes += decodedLen;
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
  printSuccess("File saved: " + filename + " (" + formatBytes(totalBytes) + ")");
}

void SerialCommands::configSet(const String& args) {
  int spaceIndex = args.indexOf(' ');
  if (spaceIndex < 0) {
    printError("Usage: /config set <key> <value>");
    return;
  }
  
  String key = args.substring(0, spaceIndex);
  String value = args.substring(spaceIndex + 1);
  
  if (!SD_MMC.begin()) {
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
  
  if (!SD_MMC.begin()) {
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
  
  JsonVariant result;
  
  // Handle nested keys (e.g., "wifi.ssid")
  if (key.indexOf('.') > 0) {
    String section = key.substring(0, key.indexOf('.'));
    String subkey = key.substring(key.indexOf('.') + 1);
    result = doc[section][subkey];
  } else {
    result = doc[key];
  }
  
  if (result.isNull()) {
    printError("Key not found: " + key);
  } else {
    Serial.printf("%s = %s\n", key.c_str(), result.as<String>().c_str());
  }
}

void SerialCommands::listFiles(const String& path) {
  if (!SD_MMC.begin()) {
    printError("SD card not available");
    return;
  }
  
  File root = SD_MMC.open(path);
  if (!root || !root.isDirectory()) {
    printError("Cannot open directory: " + path);
    return;
  }
  
  Serial.println("\nDirectory listing for: " + path);
  Serial.println("Type    Size        Name");
  Serial.println("--------------------------------");
  
  File file = root.openNextFile();
  while (file) {
    Serial.printf("%-7s %-10s %s\n", 
                  file.isDirectory() ? "DIR" : "FILE",
                  file.isDirectory() ? "" : formatBytes(file.size()).c_str(),
                  file.name());
    file = root.openNextFile();
  }
  
  root.close();
}

void SerialCommands::deleteFile(const String& path) {
  if (path.length() == 0) {
    printError("Usage: /rm <filename>");
    return;
  }
  
  if (!SD_MMC.begin()) {
    printError("SD card not available");
    return;
  }
  
  String fullPath = path.startsWith("/") ? path : ("/" + path);
  
  if (SD_MMC.remove(fullPath)) {
    printSuccess("File deleted: " + fullPath);
  } else {
    printError("Cannot delete file: " + fullPath);
  }
}

void SerialCommands::catFile(const String& path) {
  if (path.length() == 0) {
    printError("Usage: /cat <filename>");
    return;
  }
  
  if (!SD_MMC.begin()) {
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
    printError("Usage: /load <script.js>");
    return;
  }
  
  if (!SD_MMC.begin()) {
    printError("SD card not available");
    return;
  }
  
  String fullPath = scriptName.startsWith("/") ? scriptName : ("/" + scriptName);
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
  
  // Update global script filename for restart
  extern String g_script_filename;
  g_script_filename = fullPath;
  
  printSuccess("Script queued for loading: " + fullPath);
  printSuccess("Restarting to load new script...");
  delay(2000);
  ESP.restart();
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
  if (!SD_MMC.begin()) {
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
  if (!SD_MMC.begin()) {
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
      metaFile.printf("  \"version\": \"2.0.0\"\n");
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
  
  while (!Serial.available()) {
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