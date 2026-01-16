/**
 * @file binary_loader.h
 * @brief ESP32 binary firmware loader from SD card
 *
 * @details
 * This module enables loading and flashing compiled ESP32 firmware (.bin files)
 * from SD card to the OTA (Over-The-Air) partition, then rebooting into the
 * new firmware.
 *
 * Features:
 * - OTA update from SD card instead of network
 * - Binary validation and CRC checking
 * - Partition management
 * - Automatic reboot after successful flash
 * - Error recovery to current firmware on failure
 *
 * Use Case:
 * Store multiple compiled applications on SD card and switch between them
 * via webscreen.json configuration without recompiling or reflashing via USB.
 *
 * Example webscreen.json:
 * @code{.json}
 * {
 *   "settings": { "wifi": {...}, "mqtt": {...} },
 *   "screen": { "background": "#000000", "foreground": "#FFFFFF" },
 *   "bin": "/apps/sensor_dashboard.bin"  // Load this binary on boot
 * }
 * @endcode
 *
 * @warning Flashing incorrect binary can brick the device - ensure binary
 *          is compiled for the correct ESP32 variant and partition scheme
 * @note Requires sufficient OTA partition space in partition table
 * @note Original firmware can be restored by removing "bin" from webscreen.json
 *       and power cycling the device
 */

#ifndef BINARY_LOADER_H
#define BINARY_LOADER_H

/******************************************************************************
 * Includes
 ******************************************************************************/
#include <Arduino.h>
#include <SD_MMC.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

/******************************************************************************
 * Configuration
 ******************************************************************************/

#define BINARY_LOADER_BUFFER_SIZE 4096  ///< Buffer size for SD read operations
#define BINARY_LOADER_MAX_SIZE (2 * 1024 * 1024)  ///< Max binary size: 2MB

/******************************************************************************
 * Error Codes
 ******************************************************************************/

typedef enum {
  BINARY_LOADER_OK = 0,                ///< Success
  BINARY_LOADER_ERR_FILE_NOT_FOUND,    ///< Binary file not found on SD
  BINARY_LOADER_ERR_FILE_TOO_LARGE,    ///< Binary exceeds maximum size
  BINARY_LOADER_ERR_FILE_READ,         ///< Failed to read from SD card
  BINARY_LOADER_ERR_OTA_BEGIN,         ///< Failed to begin OTA update
  BINARY_LOADER_ERR_OTA_WRITE,         ///< Failed to write to OTA partition
  BINARY_LOADER_ERR_OTA_END,           ///< Failed to finalize OTA update
  BINARY_LOADER_ERR_VALIDATION,        ///< Binary validation failed
  BINARY_LOADER_ERR_NO_OTA_PARTITION,  ///< No OTA partition available
} binary_loader_error_t;

/******************************************************************************
 * Public Functions
 ******************************************************************************/

/**
 * @brief Load and flash binary firmware from SD card
 *
 * @details
 * This function performs the following steps:
 * 1. Opens binary file from SD card
 * 2. Validates file size
 * 3. Initiates OTA update process
 * 4. Reads binary in chunks and writes to OTA partition
 * 5. Validates written data
 * 6. Sets new boot partition
 * 7. Reboots into new firmware
 *
 * @param bin_path Path to .bin file on SD card (e.g., "/apps/my_app.bin")
 * @return Error code (BINARY_LOADER_OK on success)
 *
 * @note This function will reboot the device if successful
 * @note On failure, device continues running current firmware
 *
 * Example:
 * @code{.cpp}
 * binary_loader_error_t err = binary_loader_load_and_flash("/apps/sensor.bin");
 * if (err == BINARY_LOADER_OK) {
 *   // This code won't run - device will reboot
 * } else {
 *   Serial.printf("Failed to load binary: %d\n", err);
 *   // Continue with fallback behavior
 * }
 * @endcode
 */
binary_loader_error_t binary_loader_load_and_flash(const char *bin_path);

/**
 * @brief Get error description string
 *
 * @param error Error code
 * @return Human-readable error description
 *
 * Example:
 * @code{.cpp}
 * binary_loader_error_t err = binary_loader_load_and_flash("/app.bin");
 * if (err != BINARY_LOADER_OK) {
 *   Serial.println(binary_loader_get_error_string(err));
 * }
 * @endcode
 */
const char *binary_loader_get_error_string(binary_loader_error_t error);

/**
 * @brief Get information about OTA partitions
 *
 * @details Logs information about:
 * - Current running partition
 * - Next OTA partition
 * - Available space
 *
 * Useful for debugging partition issues.
 */
void binary_loader_print_partition_info();

/******************************************************************************
 * Implementation
 ******************************************************************************/

const char *binary_loader_get_error_string(binary_loader_error_t error) {
  switch (error) {
    case BINARY_LOADER_OK:
      return "Success";
    case BINARY_LOADER_ERR_FILE_NOT_FOUND:
      return "Binary file not found on SD card";
    case BINARY_LOADER_ERR_FILE_TOO_LARGE:
      return "Binary file exceeds maximum size";
    case BINARY_LOADER_ERR_FILE_READ:
      return "Failed to read binary file from SD card";
    case BINARY_LOADER_ERR_OTA_BEGIN:
      return "Failed to begin OTA update";
    case BINARY_LOADER_ERR_OTA_WRITE:
      return "Failed to write to OTA partition";
    case BINARY_LOADER_ERR_OTA_END:
      return "Failed to finalize OTA update";
    case BINARY_LOADER_ERR_VALIDATION:
      return "Binary validation failed";
    case BINARY_LOADER_ERR_NO_OTA_PARTITION:
      return "No OTA partition available";
    default:
      return "Unknown error";
  }
}

void binary_loader_print_partition_info() {
  Serial.println("=== OTA Partition Information ===");

  // Get current running partition
  const esp_partition_t *running = esp_ota_get_running_partition();
  if (running) {
    Serial.printf("Running partition: %s (type=%d, subtype=%d, size=%d bytes)\n",
                  running->label, running->type, running->subtype, running->size);
  }

  // Get next OTA partition
  const esp_partition_t *next = esp_ota_get_next_update_partition(NULL);
  if (next) {
    Serial.printf("Next OTA partition: %s (type=%d, subtype=%d, size=%d bytes)\n",
                  next->label, next->type, next->subtype, next->size);
  } else {
    Serial.println("No OTA partition available!");
  }

  // Get boot partition
  const esp_partition_t *boot = esp_ota_get_boot_partition();
  if (boot) {
    Serial.printf("Boot partition: %s\n", boot->label);
  }

  Serial.println("================================");
}

binary_loader_error_t binary_loader_load_and_flash(const char *bin_path) {
  Serial.println("=== Binary Loader ===");
  Serial.printf("Loading binary from: %s\n", bin_path);

  // Step 1: Open binary file from SD card
  File binFile = SD_MMC.open(bin_path, FILE_READ);
  if (!binFile) {
    Serial.printf("ERROR: Failed to open file: %s\n", bin_path);
    return BINARY_LOADER_ERR_FILE_NOT_FOUND;
  }

  size_t fileSize = binFile.size();
  Serial.printf("Binary file size: %u bytes (%.2f KB)\n", fileSize, fileSize / 1024.0);

  // Step 2: Validate file size
  if (fileSize == 0) {
    binFile.close();
    Serial.println("ERROR: Binary file is empty");
    return BINARY_LOADER_ERR_FILE_NOT_FOUND;
  }

  if (fileSize > BINARY_LOADER_MAX_SIZE) {
    binFile.close();
    Serial.printf("ERROR: Binary too large (%u bytes, max %u bytes)\n",
                  fileSize, BINARY_LOADER_MAX_SIZE);
    return BINARY_LOADER_ERR_FILE_TOO_LARGE;
  }

  // Step 3: Get next OTA partition
  const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
  if (!update_partition) {
    binFile.close();
    Serial.println("ERROR: No OTA partition available");
    binary_loader_print_partition_info();
    return BINARY_LOADER_ERR_NO_OTA_PARTITION;
  }

  Serial.printf("Target OTA partition: %s (size: %u bytes)\n",
                update_partition->label, update_partition->size);

  if (fileSize > update_partition->size) {
    binFile.close();
    Serial.printf("ERROR: Binary (%u bytes) exceeds partition size (%u bytes)\n",
                  fileSize, update_partition->size);
    return BINARY_LOADER_ERR_FILE_TOO_LARGE;
  }

  // Step 4: Begin OTA update
  Serial.println("Initializing OTA update...");
  if (!Update.begin(fileSize, U_FLASH)) {
    binFile.close();
    Serial.printf("ERROR: Update.begin() failed - %s\n", Update.errorString());
    return BINARY_LOADER_ERR_OTA_BEGIN;
  }

  // Step 5: Read and write binary in chunks
  Serial.println("Writing binary to OTA partition...");
  uint8_t *buffer = (uint8_t *)malloc(BINARY_LOADER_BUFFER_SIZE);
  if (!buffer) {
    binFile.close();
    Update.abort();
    Serial.println("ERROR: Failed to allocate buffer");
    return BINARY_LOADER_ERR_OTA_BEGIN;
  }

  size_t bytesWritten = 0;
  size_t lastProgressPercent = 0;

  while (binFile.available()) {
    size_t bytesRead = binFile.read(buffer, BINARY_LOADER_BUFFER_SIZE);
    if (bytesRead == 0) {
      break;  // End of file
    }

    size_t written = Update.write(buffer, bytesRead);
    if (written != bytesRead) {
      free(buffer);
      binFile.close();
      Update.abort();
      Serial.printf("ERROR: Write failed - expected %u bytes, wrote %u bytes\n",
                    bytesRead, written);
      return BINARY_LOADER_ERR_OTA_WRITE;
    }

    bytesWritten += written;

    // Print progress
    size_t progressPercent = (bytesWritten * 100) / fileSize;
    if (progressPercent >= lastProgressPercent + 10) {
      Serial.printf("Progress: %u%% (%u / %u bytes)\n",
                    progressPercent, bytesWritten, fileSize);
      lastProgressPercent = progressPercent;
    }
  }

  free(buffer);
  binFile.close();

  Serial.printf("Total bytes written: %u / %u\n", bytesWritten, fileSize);

  // Step 6: Finalize OTA update
  Serial.println("Finalizing OTA update...");
  if (!Update.end(true)) {  // true = set new boot partition
    Serial.printf("ERROR: Update.end() failed - %s\n", Update.errorString());
    return BINARY_LOADER_ERR_OTA_END;
  }

  // Step 7: Validate
  if (!Update.isFinished()) {
    Serial.println("ERROR: Update not finished");
    return BINARY_LOADER_ERR_VALIDATION;
  }

  Serial.println("=== Binary Flashed Successfully ===");
  Serial.println("Rebooting in 2 seconds...");
  delay(2000);

  // Step 8: Reboot into new firmware
  ESP.restart();

  // This code won't be reached
  return BINARY_LOADER_OK;
}

#endif  // BINARY_LOADER_H
