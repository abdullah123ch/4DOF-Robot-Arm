# Transmitter Documentation (Hand Glove)

[< Back to Project Root](../README.md)

## Overview

This Arduino sketch runs on the **ESP32** attached to the user's hand. It reads motion data from an **MPU9250 9-Axis IMU** (Gyroscope + Accelerometer) and fuses this data to determine the precise 3D orientation of the hand. It then broadcasts these angles wirelessly to the Receiver.

## Command Reference

| Function                        | Library   | Description                                |
| :------------------------------ | :-------- | :----------------------------------------- |
| `Wire.begin(SDA, SCL)`          | Wire.h    | Starts I2C communication with IMU.         |
| `Wire.setClock(400000)`         | Wire.h    | Sets I2C to Fast Mode (400kHz).            |
| `esp_now_add_peer(&peer)`       | esp_now.h | Registers the receiver as a valid target.  |
| `esp_now_send(addr, data, len)` | esp_now.h | Broadcasts the data packet.                |
| `ahrs.updateIMU(...)`           | Madgwick  | Updates sensor fusion algorithm.           |
| `map(val, in_min, ...)`         | Arduino   | Remaps a number from one range to another. |

## Code Walkthrough

### 1. I2C Communication (MPU9250)

The code manually interacts with the sensor registers to ensure maximum speed and stability.

- **`writeRegister(0x6B, 0x00)`**: Wakes the MPU9250 from sleep.
- **`readRawData(...)`**: Reads 14 bytes in a burst (Accel X/Y/Z, Temp, Gyro X/Y/Z) using `Wire.requestFrom`.

### 2. Sensor Fusion (Madgwick Filter)

Raw sensor data is noisy and drifts. The Madgwick Filter combines the quick response of the Gyroscope with the long-term stability of the Accelerometer.

- **`ahrs.updateIMU(...)`**: This function is called every loop.
  - **Inputs**: Gyro (rad/s), Accel (g), and `dt` (time since last call).
  - **Math**: It uses Quaternions (`q0, q1, q2, q3`) to represent 3D rotation.

### 3. ESP-NOW Wireless Setup

- **`WiFi.mode(WIFI_STA)`**: ESP-NOW requires Wi-Fi Station Mode.
- **`esp_wifi_set_channel(1, ...)`**: Locks the Wi-Fi channel to Channel 1. This is critical for stability; if the channel hops, the connection breaks.
- **`esp_now_send(...)`**: Blasts the data packet into the air.
