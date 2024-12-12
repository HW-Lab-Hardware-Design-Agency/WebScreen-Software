# WebScreen Firmware

**WebScreen - Stay Connected, Stay Focused - Subscribe: https://www.crowdsupply.com/hw-media-lab/webscreen**

## Project Description

WebScreen is an open-source project that provides firmware for a customizable secondary display that sits atop your monitor like a webcam. Powered by an ESP32-S3 microcontroller and featuring a vibrant AMOLED display, WebScreen brings essential notifications, reminders, and visuals directly into your line of sight. This repository contains the source code and instructions to build and customize your own WebScreen device.

## Features

- **AMOLED Display**: High-quality display with 240 x 536 resolution and 16.7 million colors.
- **ESP32-S3 Microcontroller**: Ensures smooth operation and robust performance.
- **Wi-Fi and Bluetooth Connectivity**: Stay connected with wireless capabilities.
- **Customizable Firmware**: Open-source code allows for personal modifications and enhancements.
- **Micro SD Card Support**: Load custom JavaScript applications and store additional data.
- **LVGL Graphics Library**: Advanced graphics handling for a rich user interface.

## Hardware Requirements

- **ESP32-S3 Development Board**
- **RM67162 AMOLED Display**
- **Micro SD Card (optional)**
- **Supporting Components**: Resistors, capacitors, connectors as per the BOM.

## Software Requirements

- **Arduino IDE**: For compiling and uploading the firmware.
- **LVGL Library**: Graphics library for the display ([LVGL GitHub](https://github.com/lvgl/lvgl)).
- **Elk JS Engine**: Lightweight JavaScript engine for embedded systems.
- **Wi-Fi and Networking Libraries**: For network connectivity.

## Installation Instructions

1. **Set Up Arduino IDE**:
   - Install the latest version of the [Arduino IDE](https://www.arduino.cc/en/software).
   - Add the ESP32 board support to your Arduino IDE. Instructions can be found [here](https://github.com/espressif/arduino-esp32/blob/master/docs/arduino-ide/boards_manager.md).

2. **Clone This Repository**:
   ```bash
   https://github.com/HW-Lab-Hardware-Design-Agency/WebScreen-Software.git
   ```

3. **Install Required Libraries**:
   - Open the Arduino IDE and navigate to **Sketch** > **Include Library** > **Manage Libraries**.
   - Install the following libraries:
     - **LVGL**
     - **WiFi**
     - **ESPping**
     - **ArduinoJson**
     - **YAMLDuino**
     - **SD_MMC**

4. **Configure Pin Definitions**:
   - Review and modify `pins_config.h` according to your hardware setup.

5. **Compile and Upload**:
   - Open the desired `.ino` file in the Arduino IDE.
   - Connect your ESP32-S3 board to your computer via USB.
   - Select the correct board and port under **Tools**.
   - Click **Upload** to compile and upload the firmware to your device.

## Usage Instructions

- **Connecting to Wi-Fi**:
  - The firmware reads Wi-Fi credentials from the `/webscreen.yml` file on the SD card.
  - Ensure your SD card contains the `webscreen.yml` file with your network SSID and password.

- **Interacting with the Display**:
  - The device displays notifications and visuals based on the firmware logic.
  - You can customize the displayed content by modifying the firmware or loading custom JavaScript applications via the SD card.

- **Running JavaScript Applications**:
  - Place your JavaScript files on the SD card.
  - The Elk JS engine will execute the scripts, allowing for dynamic functionality.

## File Structure

- `main.cpp`: The main firmware file containing the setup and loop functions.
- `pins_config.h`: Pin definitions and configurations.
- `rm67162.h` / `rm67162.cpp`: Driver code for the RM67162 AMOLED display.
- `elk.h`: Header file for the Elk JS engine.
- `notification.h`: Header file for the notification image resource.
- `other .ino and .cpp files`: Additional examples and functionalities.

## Contributing

We welcome contributions from the community! If you'd like to contribute:

1. Fork the repository.
2. Create a new branch for your feature or bugfix.
3. Commit your changes and push to your fork.
4. Create a pull request detailing your changes.

## License

This project is licensed under the **MIT License** - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- **LVGL**: Open-source graphics library.
- **Elk JS Engine**: Embedded JavaScript engine.
- **Arduino Community**: For the development environment and extensive libraries.
