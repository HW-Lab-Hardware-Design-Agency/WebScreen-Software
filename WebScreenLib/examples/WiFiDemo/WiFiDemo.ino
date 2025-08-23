/**
 * @file WiFiDemo.ino
 * @brief WebScreen WiFi connectivity demonstration
 * 
 * This example shows how to use WebScreen's WiFi capabilities including:
 * - Connecting to WiFi networks
 * - Making HTTP requests
 * - Handling network connectivity issues
 * - Monitoring network status
 * 
 * Hardware Required:
 * - WebScreen device (ESP32-S3 with display)
 * - WiFi network access
 * 
 * Created by WebScreen Project
 * This example code is in the public domain.
 */

#include <WebScreen.h>

// Create WebScreen instance
WebScreen webscreen;

// Network configuration
const char* ssid = "YourWiFiNetwork";
const char* password = "YourWiFiPassword";

// HTTP endpoints for testing
const char* test_endpoints[] = {
    "http://httpbin.org/get",
    "http://api.github.com",
    "http://worldtimeapi.org/api/timezone/Etc/UTC"
};
const int num_endpoints = sizeof(test_endpoints) / sizeof(test_endpoints[0]);

// Timing variables
unsigned long last_status_check = 0;
unsigned long last_http_test = 0;
int current_endpoint = 0;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 5000) {
        delay(10);
    }
    
    Serial.println("WebScreen WiFi Demo");
    Serial.println("==================");
    
    // Configure WebScreen
    webscreen.setWiFi(ssid, password);
    webscreen.setDisplay(150, 1, 0x000080, 0x00FFFF); // Dimmer for power saving
    
    // Initialize WebScreen
    Serial.println("Initializing WebScreen...");
    if (!webscreen.begin()) {
        Serial.println("ERROR: WebScreen initialization failed!");
        while (true) {
            delay(1000);
        }
    }
    
    Serial.println("WebScreen initialized!");
    Serial.println();
    
    // Print network information
    printNetworkInfo();
    
    Serial.println("Starting WiFi connectivity tests...");
    Serial.println("Commands: 'status', 'test', 'reconnect', 'scan'");
    Serial.println();
}

void loop() {
    // Run WebScreen main loop
    webscreen.loop();
    
    // Periodic network status checks
    if (millis() - last_status_check > 30000) { // Every 30 seconds
        last_status_check = millis();
        checkNetworkHealth();
    }
    
    // Periodic HTTP tests
    if (millis() - last_http_test > 60000) { // Every 60 seconds
        last_http_test = millis();
        runHttpTest();
    }
    
    // Handle serial commands
    handleSerialCommands();
    
    delay(100);
}

/**
 * @brief Print detailed network information
 */
void printNetworkInfo() {
    Serial.println("\n=== NETWORK INFORMATION ===");
    
    if (webscreen.isWiFiConnected()) {
        Serial.printf("WiFi Status: Connected\n");
        Serial.printf("SSID: %s\n", WiFi.SSID().c_str());
        Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
        Serial.printf("DNS: %s\n", WiFi.dnsIP().toString().c_str());
        Serial.printf("MAC Address: %s\n", WiFi.macAddress().c_str());
        Serial.printf("Signal Strength: %d dBm\n", WiFi.RSSI());
        Serial.printf("Channel: %d\n", WiFi.channel());
    } else {
        Serial.printf("WiFi Status: Disconnected\n");
        Serial.printf("Last Status: %d\n", WiFi.status());
    }
    
    Serial.println("==============================\n");
}

/**
 * @brief Check network health and connectivity
 */
void checkNetworkHealth() {
    Serial.println("Checking network health...");
    
    if (!webscreen.isWiFiConnected()) {
        Serial.println("WARNING: WiFi connection lost!");
        webscreen.setFallbackText("WiFi Disconnected\nAttempting reconnection...");
        return;
    }
    
    // Test basic connectivity
    String response = webscreen.httpGet("http://httpbin.org/ip");
    if (response.length() > 0) {
        Serial.println("Network connectivity: OK");
        
        // Parse and display IP information
        if (response.indexOf("origin") > 0) {
            int start = response.indexOf("\"") + 1;
            int end = response.indexOf("\"", start);
            String public_ip = response.substring(start, end);
            Serial.printf("Public IP: %s\n", public_ip.c_str());
            
            String status_text = "WiFi Connected\nIP: " + WiFi.localIP().toString() + 
                               "\nPublic: " + public_ip;
            webscreen.setFallbackText(status_text.c_str());
        }
    } else {
        Serial.println("WARNING: Network connectivity test failed!");
        webscreen.setFallbackText("WiFi Connected\nBut no internet access");
    }
}

/**
 * @brief Run HTTP connectivity tests
 */
void runHttpTest() {
    if (!webscreen.isWiFiConnected()) {
        return;
    }
    
    Serial.printf("Testing HTTP endpoint %d/%d: %s\n", 
                  current_endpoint + 1, num_endpoints, test_endpoints[current_endpoint]);
    
    unsigned long start_time = millis();
    String response = webscreen.httpGet(test_endpoints[current_endpoint]);
    unsigned long request_time = millis() - start_time;
    
    if (response.length() > 0) {
        Serial.printf("✓ Request successful (%lu ms, %d bytes)\n", 
                      request_time, response.length());
        
        // Show first 100 characters of response
        String preview = response.substring(0, 100);
        preview.replace("\n", " ");
        Serial.printf("Response preview: %s%s\n", 
                      preview.c_str(), response.length() > 100 ? "..." : "");
    } else {
        Serial.printf("✗ Request failed (%lu ms)\n", request_time);
    }
    
    // Move to next endpoint
    current_endpoint = (current_endpoint + 1) % num_endpoints;
}

/**
 * @brief Scan for available WiFi networks
 */
void scanWiFiNetworks() {
    Serial.println("Scanning for WiFi networks...");
    
    int networks = WiFi.scanNetworks();
    if (networks == 0) {
        Serial.println("No networks found");
        return;
    }
    
    Serial.printf("Found %d networks:\n", networks);
    Serial.println("ID | SSID                | Signal | Encryption");
    Serial.println("---|---------------------|--------|------------");
    
    for (int i = 0; i < networks; i++) {
        Serial.printf("%2d | %-20s | %3d dBm | %s\n",
                      i + 1,
                      WiFi.SSID(i).c_str(),
                      WiFi.RSSI(i),
                      getEncryptionType(WiFi.encryptionType(i)));
    }
    
    // Clean up
    WiFi.scanDelete();
}

/**
 * @brief Get human-readable encryption type
 */
const char* getEncryptionType(wifi_auth_mode_t encryptionType) {
    switch (encryptionType) {
        case WIFI_AUTH_OPEN: return "Open";
        case WIFI_AUTH_WEP: return "WEP";
        case WIFI_AUTH_WPA_PSK: return "WPA";
        case WIFI_AUTH_WPA2_PSK: return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-EAP";
        case WIFI_AUTH_WPA3_PSK: return "WPA3";
        case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3";
        default: return "Unknown";
    }
}

/**
 * @brief Handle serial debug commands
 */
void handleSerialCommands() {
    if (!Serial.available()) {
        return;
    }
    
    String command = Serial.readStringUntil('\n');
    command.trim();
    command.toLowerCase();
    
    if (command == "status") {
        printNetworkInfo();
        webscreen.printSystemInfo();
        
    } else if (command == "test") {
        runHttpTest();
        
    } else if (command == "reconnect") {
        Serial.println("Forcing WiFi reconnection...");
        WiFi.disconnect();
        delay(1000);
        WiFi.begin(ssid, password);
        
        Serial.print("Reconnecting");
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(500);
            Serial.print(".");
            attempts++;
        }
        Serial.println();
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("Reconnection successful!");
            printNetworkInfo();
        } else {
            Serial.println("Reconnection failed!");
        }
        
    } else if (command == "scan") {
        scanWiFiNetworks();
        
    } else if (command == "ping") {
        // Simple connectivity test
        Serial.println("Testing connectivity...");
        String response = webscreen.httpGet("http://httpbin.org/get");
        if (response.length() > 0) {
            Serial.println("✓ Internet connectivity OK");
        } else {
            Serial.println("✗ Internet connectivity failed");
        }
        
    } else if (command == "speed") {
        // Speed test (simple)
        Serial.println("Running simple speed test...");
        
        const char* test_url = "http://httpbin.org/bytes/10240"; // 10KB
        unsigned long start = millis();
        String response = webscreen.httpGet(test_url);
        unsigned long duration = millis() - start;
        
        if (response.length() > 0) {
            float speed_kbps = (response.length() * 8.0) / duration; // kbps
            Serial.printf("Downloaded %d bytes in %lu ms (%.2f kbps)\n", 
                          response.length(), duration, speed_kbps);
        } else {
            Serial.println("Speed test failed");
        }
        
    } else if (command == "help") {
        Serial.println("Available commands:");
        Serial.println("  status    - Show network status");
        Serial.println("  test      - Test HTTP endpoint");
        Serial.println("  reconnect - Force WiFi reconnection");
        Serial.println("  scan      - Scan for WiFi networks");
        Serial.println("  ping      - Test internet connectivity");
        Serial.println("  speed     - Run simple speed test");
        Serial.println("  help      - Show this help");
        
    } else if (command.length() > 0) {
        Serial.printf("Unknown command: %s\n", command.c_str());
        Serial.println("Type 'help' for available commands.");
    }
}