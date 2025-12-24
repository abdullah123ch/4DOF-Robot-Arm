# Receiver Documentation (ESP32)

## Overview

This component is the **Wireless Bridge**. Its purpose is to receive wireless packets from the Hand Glove (Transmitter) via **ESP-NOW** and convert them into wired **UART Serial** commands for the Robot (TM4C123).

> **Note**: The current source file `sketch_dec24a.ino` is empty. The instructions below describe the **required implementation**.

## Required logic

### 1. Libraries

The code must include:

- `#include <esp_now.h>`: For the wireless protocol.
- `#include <WiFi.h>`: ESP-NOW requires the WiFi radio to be active.

### 2. Initialization (`setup`)

- **`Serial.begin(115200, SERIAL_8N1, 16, 17)`**: Initialize UART to talk to the Robot. Note: You must use pins compatible with the Robot's UART input.
- **`WiFi.mode(WIFI_STA)`**: Set Station mode (required for ESP-NOW).
- **`esp_now_init()`**: Starts the ESP-NOW stack.
- **`esp_now_register_recv_cb(OnDataRecv)`**: Registers a "Callback Function" that triggers instantly when a packet arrives.

### 3. The Callback Function (`OnDataRecv`)

This is where the magic happens.

```cpp
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
    memcpy(&myData, incomingData, sizeof(myData));

    // Construct Packet for Robot
    Serial.write(0xFF);            // Header
    Serial.write(myData.base);     // Data
    Serial.write(myData.shoulder);
    Serial.write(myData.elbow);
    Serial.write(myData.claw);
    Serial.write(0xFE);            // Footer
}
```

- **`memcpy`**: Copies the raw bytes from the wireless buffer into your data structure.
- **`Serial.write`**: Sends the binary data out the Tx pin to the TM4C123.

## Why use ESP-NOW?

- **Low Latency**: Much faster than standard Wi-Fi or Bluetooth.
- **Peer-to-Peer**: No router needed. The two ESP32s talk directly to each other.
