#pragma once

#include <Arduino.h>
#include <SD_MMC.h>
#include <ArduinoJson.h>

class SerialCommands {
public:
  static void init();
  static void processCommand(const String& command);
  
private:
  static void showHelp();
  static void showStats();
  static void showInfo();
  static void writeScript(const String& args);
  static void configSet(const String& args);
  static void configGet(const String& args);
  static void listFiles(const String& path);
  static void deleteFile(const String& path);
  static void catFile(const String& path);
  static void reboot();
  
  static void printPrompt();
  static String formatBytes(size_t bytes);
  static void printError(const String& message);
  static void printSuccess(const String& message);
};