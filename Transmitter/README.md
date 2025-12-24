# Transmitter Documentation (Hand Glove)

## Overview

This Arduino sketch runs on the **ESP32** attached to the user's hand. It reads motion data from an **MPU9250 9-Axis IMU** (Gyroscope + Accelerometer) and fuses this data to determine the precise 3D orientation of the hand. It then broadcasts these angles wirelessly to the Receiver.

## Dependencies

- **Wire.h**: Handles I2C communication with the MPU9250 sensor.
- **WiFi.h & esp_now.h**: Handles the wireless P2P communication.
- **MadgwickFilter (Internal Class)**: Defines the sensor fusion algorithm.

## Code Walkthrough & Command Explanation

### 1. I2C Communication (MPU9250)

The code manually interacts with the sensor registers to ensure maximum speed and stability.

- **`Wire.begin(I2C_SDA, I2C_SCL)`**: Starts the I2C bus on the specified pins (21, 22).
- **`Wire.setClock(400000)`**: Sets speed to 400kHz (Fast Mode).
- **`writeRegister(0x6B, 0x00)`**: Wakes the MPU9250 from sleep.
- **`writeRegister(0x37, 0x02)`**: Enables "Bypass Mode" (optional, for accessing the magnetometer directly).
- **`readRawData(...)`**: Reads 14 bytes in a burst (Accel X/Y/Z, Temp, Gyro X/Y/Z) using `Wire.requestFrom`.

### 2. Sensor Fusion (Madgwick Filter)

Raw sensor data is noisy and drifts. The Madgwick Filter combines the quick response of the Gyroscope with the long-term stability of the Accelerometer.

- **`ahrs.updateIMU(...)`**: This function is called every loop.
  - **Inputs**: Gyro (rad/s), Accel (g), and `dt` (time since last call).
  - **Math**: It uses Quaternions (`q0, q1, q2, q3`) to represent 3D rotation without "Gimbal Lock".
- **`ahrs.getRoll() / getPitch() / getYaw()`**: Converts the quaternion back into angles (degrees) for the servos.

### 3. ESP-NOW Wireless Setup

- **`WiFi.mode(WIFI_STA)`**: ESP-NOW requires Wi-Fi Station Mode.
- **`esp_wifi_set_channel(1, ...)`**: Locks the Wi-Fi channel to Channel 1. This is critical for stability; if the channel hops, the connection breaks.
- **`esp_now_init()`**: Starts the protocol stack.
- **`esp_now_add_peer(&peerInfo)`**: Registers the Receiver's MAC address so the ESP32 knows where to send data.

### 4. Main Looop & Sending

```cpp
if (millis() - lastPrint >= 40) { ... }
```

- The loop runs at ~25Hz (every 40ms). This matches the human reaction time and servo update limits.
- **`map(smoothYaw, -70, 70, 0, 180)`**: Converts the hand angle (-70 to +70 degrees) to the Servo angle (0 to 180).
- **`constrain(...)`**: Ensures the value never signals outside valid servo bounds.
- **`esp_now_send(...)`**: Blasts the data packet into the air.

### 5. Gyro Calibration

```cpp
void calibrateGyro() { ... }
```

- Runs once on startup.
- Takes 1000 samples while the hand is still.
- Averages them to find the "Zero Offset" (Bias) and subtracts this from all future readings to prevent drift.
