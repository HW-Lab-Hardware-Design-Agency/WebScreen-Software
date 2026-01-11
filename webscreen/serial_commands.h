#pragma once

#include <Arduino.h>
#include <SD_MMC.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

class SerialCommands {
public:
  static void init();
  static void processCommand(const String& command);
  
private:
  static void showHelp();
  static void showStats();
  static void showInfo();
  static void writeScript(const String& args);
  static void uploadFile(const String& args);
  static void configSet(const String& args);
  static void configGet(const String& args);
  static void listFiles(const String& path);
  static void deleteFile(const String& path);
  static void catFile(const String& path);
  static void reboot();
  static void loadApp(const String& scriptName);
  static void wget(const String& args);
  static void ping(const String& args);
  static void backup(const String& args);
  static void monitor(const String& args);
  
  static void printPrompt();
  static String formatBytes(size_t bytes);
  static void printError(const String& message);
  static void printSuccess(const String& message);
};