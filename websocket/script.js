// script.js

// Enable strict mode
"use strict";

// Print a message
print("Hello from Elk JavaScript!");

// Declare all variables at the top level
let ssid = "Personal-80E-2.4GHz";
let password = "17EF32280E";
let connected;
let ip;
let fileList;
let writeSuccess;
let ledPin = 10;  // Adjust pin number as needed
let i;

// Connect to Wi-Fi
print("Connecting to Wi-Fi...");
connected = wifi_connect(ssid, password);

// Debugging: Print the value of 'connected'
print("connected =");
print(connected);

if (connected) {
  print("Connected to Wi-Fi");

  // Get IP address
  ip = wifi_get_ip();
  print("IP Address:");
  print(ip);
} else {
  print("Failed to connect to Wi-Fi");
}

print("After Wi-Fi connection attempt");

// Check Wi-Fi status
if (wifi_status()) {
  print("Wi-Fi is connected");
} else {
  print("Wi-Fi is not connected");
}

print("Before listing directory");

// List files in the root directory
fileList = sd_list_dir("/");
print("After sd_list_dir call");

print("fileList is:");
print(fileList);

if (fileList) {
  print("Directory contents:");
  print(fileList);
} else {
  print("Failed to list directory");
}

print("After listing directory");

// Optionally, proceed with other operations
print("Script execution completed");

