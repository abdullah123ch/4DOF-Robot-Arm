# Receiver Documentation (ESP32)

[< Back to Project Root](../README.md)

## Overview

This component is the **Wireless Bridge**. Its purpose is to receive wireless packets from the Hand Glove (Transmitter) via **ESP-NOW** and convert them into wired **UART Serial** commands for the Robot (TM4C123).

## Command Reference

| Function                     | Library   | Description                               |
| :--------------------------- | :-------- | :---------------------------------------- |
| `esp_now_init()`             | esp_now.h | Initializes the ESP-NOW protocol stack.   |
| `WiFi.mode(WIFI_STA)`        | WiFi.h    | Sets chip to Station Mode (Required).     |
| `esp_now_register_recv_cb()` | esp_now.h | Registers the callback for incoming data. |
| `Serial.begin(115200)`       | Arduino   | Starts Serial UART.                       |
| `Serial.write(byte)`         | Arduino   | Sends a binary byte to the Robot.         |
| `memcpy(dest, src, size)`    | C++       | Copies raw memory (struct data).          |

## Required logic

### 1. Libraries

The code must include:

- `#include <esp_now.h>`: For the wireless protocol.
- `#include <WiFi.h>`: ESP-NOW requires the WiFi radio to be active.

### 2. Initialization (`setup`)

- **`Serial.begin(115200, SERIAL_8N1, 16, 17)`**: Initialize UART to talk to the Robot. Note: You must use pins compatible with the Robot's UART input.
- **`WiFi.mode(WIFI_STA)`**: Set Station mode (required for ESP-NOW).
- **`esp_now_init()`**: Starts the ESP-NOW stack.

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
