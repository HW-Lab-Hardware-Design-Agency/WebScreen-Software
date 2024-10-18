#include <Arduino.h>
#include <WiFi.h>
#include <FS.h>
#include <SD_MMC.h>  // For SD card access
#include <stdio.h>

// Define SD card pins (adjust these pins according to your hardware setup)
#define PIN_SD_CMD 13  // CMD
#define PIN_SD_CLK 11  // CLK
#define PIN_SD_D0  12  // Data0

// Include elk.h with extern "C" to ensure correct linkage
extern "C" {
#include "elk.h"
}

// Ensure that 'elk.c' is included in your project and being compiled.

// Elk JavaScript engine instance
struct js *js;

// Native function to print to the Serial Monitor
static jsval_t js_print(struct js *js, jsval_t *args, int nargs) {
  for (int i = 0; i < nargs; i++) {
    const char *str = js_str(js, args[i]);
    if (str != NULL) {
      Serial.println(str);
    } else {
      Serial.println("print: argument is not a string");
    }
  }
  return js_mknull();
}

// Native function to connect to Wi-Fi
static jsval_t js_wifi_connect(struct js *js, jsval_t *args, int nargs) {
  if (nargs != 2) {
    Serial.println("wifi_connect: expected 2 arguments");
    return js_mkfalse();
  }
  const char *ssid_with_quotes = js_str(js, args[0]);
  const char *password_with_quotes = js_str(js, args[1]);
  if (ssid_with_quotes == NULL || password_with_quotes == NULL) {
    Serial.println("wifi_connect: arguments must be strings");
    return js_mkfalse();
  }

  // Strip the surrounding quotes from SSID and password
  String ssid = String(ssid_with_quotes);
  if (ssid.startsWith("\"") && ssid.endsWith("\"")) {
    ssid = ssid.substring(1, ssid.length() - 1);
  }

  String password = String(password_with_quotes);
  if (password.startsWith("\"") && password.endsWith("\"")) {
    password = password.substring(1, password.length() - 1);
  }

  Serial.printf("Connecting to Wi-Fi SSID: %s\n", ssid.c_str());
  WiFi.begin(ssid.c_str(), password.c_str());

  int max_attempts = 20;
  while (WiFi.status() != WL_CONNECTED && max_attempts > 0) {
    delay(500);
    Serial.print(".");
    max_attempts--;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Wi-Fi connected");
    return js_mktrue();
  } else {
    Serial.println("Failed to connect to Wi-Fi");
    return js_mkfalse();
  }
}

// Native function to check Wi-Fi connection status
static jsval_t js_wifi_status(struct js *js, jsval_t *args, int nargs) {
  return (WiFi.status() == WL_CONNECTED) ? js_mktrue() : js_mkfalse();
}

// Native function to delay execution
static jsval_t js_delay(struct js *js, jsval_t *args, int nargs) {
  if (nargs != 1) {
    Serial.println("delay: expected 1 argument");
    return js_mknull();
  }
  double ms = js_getnum(args[0]);
  delay((unsigned long)ms);
  return js_mknull();
}

// Native function to read a file from the SD card
static jsval_t js_sd_read_file(struct js *js, jsval_t *args, int nargs) {
  if (nargs != 1) {
    Serial.println("sd_read_file: expected 1 argument");
    return js_mknull();
  }
  const char *path = js_str(js, args[0]);
  if (path == NULL) {
    Serial.println("sd_read_file: argument is not a string");
    return js_mknull();
  }

  File file = SD_MMC.open(path);
  if (!file) {
    Serial.println("Failed to open file");
    return js_mknull();
  }

  String content = file.readString();
  file.close();

  return js_mkstr(js, content.c_str(), content.length());
}

// Native function to write data to a file on the SD card
static jsval_t js_sd_write_file(struct js *js, jsval_t *args, int nargs) {
  if (nargs != 2) {
    Serial.println("sd_write_file: expected 2 arguments");
    return js_mkfalse();
  }
  const char *path = js_str(js, args[0]);
  const char *data = js_str(js, args[1]);
  if (path == NULL || data == NULL) {
    Serial.println("sd_write_file: arguments must be strings");
    return js_mkfalse();
  }

  File file = SD_MMC.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return js_mkfalse();
  }

  file.write((const uint8_t *)data, strlen(data));
  file.close();

  return js_mktrue();
}

// Native function to list files in a directory on the SD card
static jsval_t js_sd_list_dir(struct js *js, jsval_t *args, int nargs) {
  if (nargs != 1) {
    Serial.println("sd_list_dir: expected 1 argument");
    return js_mknull();
  }
  const char *path_with_quotes = js_str(js, args[0]);
  if (path_with_quotes == NULL) {
    Serial.println("sd_list_dir: argument is not a string");
    return js_mknull();
  }

  // Strip the surrounding quotes from path
  String path = String(path_with_quotes);
  if (path.startsWith("\"") && path.endsWith("\"")) {
    path = path.substring(1, path.length() - 1);
  }

  File root = SD_MMC.open(path);
  if (!root) {
    Serial.println("Failed to open directory");
    return js_mknull();
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    root.close();
    return js_mknull();
  }

  // Use a fixed-size buffer
  char fileList[512];  // Adjust size as needed
  int fileListLen = 0;

  File file = root.openNextFile();
  while (file) {
    const char* type = file.isDirectory() ? "DIR: " : "FILE: ";
    const char* name = file.name();
    int len = snprintf(fileList + fileListLen, sizeof(fileList) - fileListLen, "%s%s\n", type, name);
    if (len < 0 || len >= (int)(sizeof(fileList) - fileListLen)) {
      // Buffer full, break
      break;
    }
    fileListLen += len;
    file = root.openNextFile();
  }
  root.close();

  return js_mkstr(js, fileList, fileListLen);
}

// Native function to get the IP address
static jsval_t js_wifi_get_ip(struct js *js, jsval_t *args, int nargs) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Not connected to Wi-Fi");
    return js_mknull();
  }

  IPAddress ip = WiFi.localIP();
  String ipStr = ip.toString();
  return js_mkstr(js, ipStr.c_str(), ipStr.length());
}

// Native function to set GPIO mode
static jsval_t js_gpio_mode(struct js *js, jsval_t *args, int nargs) {
  if (nargs != 2) {
    Serial.println("gpio_mode: expected 2 arguments");
    return js_mknull();
  }
  int pin = (int)js_getnum(args[0]);
  int mode = (int)js_getnum(args[1]);
  pinMode(pin, mode);
  return js_mknull();
}

// Native function to write to a GPIO pin
static jsval_t js_gpio_write(struct js *js, jsval_t *args, int nargs) {
  if (nargs != 2) {
    Serial.println("gpio_write: expected 2 arguments");
    return js_mknull();
  }
  int pin = (int)js_getnum(args[0]);
  int value = (int)js_getnum(args[1]);
  digitalWrite(pin, value);
  return js_mknull();
}

// Function to register native functions with Elk
void register_js_functions() {
  jsval_t global = js_glob(js);

  // Register global functions
  js_set(js, global, "print", js_mkfun(js_print));
  js_set(js, global, "wifi_connect", js_mkfun(js_wifi_connect));
  js_set(js, global, "wifi_status", js_mkfun(js_wifi_status));
  js_set(js, global, "sd_read_file", js_mkfun(js_sd_read_file));
  js_set(js, global, "sd_write_file", js_mkfun(js_sd_write_file));
  js_set(js, global, "wifi_get_ip", js_mkfun(js_wifi_get_ip));
  js_set(js, global, "delay", js_mkfun(js_delay));
  js_set(js, global, "sd_list_dir", js_mkfun(js_sd_list_dir));  // Add this line

  // Create 'gpio' object
  jsval_t gpio = js_mkobj(js);
  js_set(js, global, "gpio", gpio);
  js_set(js, gpio, "mode", js_mkfun(js_gpio_mode));
  js_set(js, gpio, "write", js_mkfun(js_gpio_write));
}

// Function to load and execute a JavaScript script from the SD card
bool load_and_execute_js_script(const char* path) {
  Serial.printf("Loading JavaScript script from: %s\n", path);

  File file = SD_MMC.open(path);
  if (!file) {
    Serial.println("Failed to open JavaScript script file");
    return false;
  }

  // Read the entire file into a String
  String jsScript = file.readString();
  file.close();

  // Execute the JavaScript script
  jsval_t res = js_eval(js, jsScript.c_str(), jsScript.length());
  if (js_type(res) == JS_ERR) {
    const char *error = js_str(js, res);
    Serial.printf("Error executing script: %s\n", error);
    return false;
  }

  Serial.println("JavaScript script executed successfully");
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Set SD card pins
  SD_MMC.setPins(PIN_SD_CLK, PIN_SD_CMD, PIN_SD_D0);

  // Initialize SD card
  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("Card Mount Failed");
    return;
  }

  // Initialize Wi-Fi
  WiFi.mode(WIFI_STA);

  // Initialize Elk with a memory buffer
  static uint8_t elk_memory[4096];  // Adjust size as needed
  js = js_create(elk_memory, sizeof(elk_memory));
  if (js == NULL) {
    Serial.println("Failed to initialize Elk");
    return;
  }

  // Register functions with Elk
  register_js_functions();

  // Load and execute the JavaScript script from the SD card
  if (!load_and_execute_js_script("/script.js")) {
    Serial.println("Failed to load and execute JavaScript script");
  }
}

void loop() {
  // Your main code here
}
