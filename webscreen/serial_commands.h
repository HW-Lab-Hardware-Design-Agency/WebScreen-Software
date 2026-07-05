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
  // Command table for dispatch and /help (defined in serial_commands.cpp)
  struct Command;
  static const Command kCommands[];
  static const size_t kCommandCount;

  static void showHelp();
  static void showStats();
  static void showInfo();
  static void writeScript(const String& args);
  static void uploadFile(const String& args);
  static void configCommand(const String& args);
  static void configSet(const String& args);
  static void configGet(const String& args);
  static void listFiles(const String& path);
  static void deleteFile(const String& path);
  static void catFile(const String& path);
  static void makeDirectory(const String& path);
  static void downloadFile64(const String& path);
  static void factoryReset(const String& args);
  static void screenshot();
  static void reboot();
  static void loadApp(const String& scriptName);
  static void restartApp();
  static void runGC();
  static void evalJs(const String& args);
  static void showErrors();
  static bool sdReady();
  static void wget(const String& args);
  static void ping(const String& args);
  static void backup(const String& args);
  static void monitor(const String& args);
  static void setBrightness(const String& args);
  static void showTime();
  static void setTime(const String& args);

  static void printPrompt();
  static String formatBytes(size_t bytes);
  static void printError(const String& message);
  static void printSuccess(const String& message);
};