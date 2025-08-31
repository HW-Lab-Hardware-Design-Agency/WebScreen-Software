#include "serial_commands.h"
#include "globals.h"
#include <WiFi.h>
#include <esp_system.h>

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
  Serial.println("/config get <key>        - Get config value from webscreen.json");
  Serial.println("/config set <key> <val>  - Set config value in webscreen.json");
  Serial.println("/ls [path]               - List files/directories");
  Serial.println("/cat <file>              - Display file contents");
  Serial.println("/rm <file>               - Delete file");
  Serial.println("/load <script.js>        - Load/switch to different JS app");
  Serial.println("/reboot                  - Restart the device");
  Serial.println("\nExamples:");
  Serial.println("/write hello.js");
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