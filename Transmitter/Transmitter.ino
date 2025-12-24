#include <Wire.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

// ========== CONFIGURATION ==========
#define I2C_SDA 21
#define I2C_SCL 22
#define MPU_ADDR 0x68
#define CALIBRATION_SAMPLES 1000
#define FILTER_BETA 0.1f
#define SMOOTHING_ALPHA 0.1f
#define CLAW_BUTTON_PIN 0 

// 🎯 TARGET MAC ADDRESS (Change this to your Receiver's MAC)
uint8_t broadcastAddress[] = {0xCC, 0x7B, 0x5C, 0x35, 0x24, 0x90}; 

typedef struct struct_message {
    uint8_t base;
    uint8_t shoulder;
    uint8_t elbow;
    uint8_t claw;
} struct_message;

struct_message myData;
esp_now_peer_info_t peerInfo;

class MadgwickFilter {
private:
    float q0 = 1.0f, q1 = 0.0f, q2 = 0.0f, q3 = 0.0f;
    float beta = FILTER_BETA;
public:
    void updateIMU(float gx, float gy, float gz, float ax, float ay, float az, float dt) {
        gx *= 0.0174533f; gy *= 0.0174533f; gz *= 0.0174533f;
        float norm = sqrtf(ax * ax + ay * ay + az * az);
        if (norm < 0.001f) return;
        norm = 1.0f / norm; ax *= norm; ay *= norm; az *= norm;
        float _2q0 = 2.0f * q0, _2q1 = 2.0f * q1, _2q2 = 2.0f * q2, _2q3 = 2.0f * q3;
        float _4q0 = 4.0f * q0, _4q1 = 4.0f * q1, _4q2 = 4.0f * q2, q0q0 = q0 * q0, q1q1 = q1 * q1, q2q2 = q2 * q2, q3q3 = q3 * q3;
        float s0 = _4q0 * q2q2 + _2q2 * ax + _4q0 * q1q1 - _2q1 * ay;
        float s1 = _4q1 * q3q3 - _2q3 * ax + 4.0f * q0q0 * q1 - _2q0 * ay - _4q1 + 8.0f * q1 * q1q1 + 8.0f * q1 * q2q2 + _4q1 * az;
        float s2 = 4.0f * q0q0 * q2 + _2q0 * ax + _4q2 * q3q3 - _2q3 * ay - _4q2 + 8.0f * q2 * q1q1 + 8.0f * q2 * q2q2 + _4q2 * az;
        float s3 = 4.0f * q1q1 * q3 - _2q1 * ax + 4.0f * q2q2 * q3 - _2q2 * ay;
        norm = 1.0f / sqrtf(s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3);
        s0 *= norm; s1 *= norm; s2 *= norm; s3 *= norm;
        float qDot1 = 0.5f * (-q1 * gx - q2 * gy - q3 * gz) - beta * s0;
        float qDot2 = 0.5f * (q0 * gx + q2 * gz - q3 * gy) - beta * s1;
        float qDot3 = 0.5f * (q0 * gy - q1 * gz + q3 * gx) - beta * s2;
        float qDot4 = 0.5f * (q0 * gz + q1 * gy - q2 * gx) - beta * s3;
        q0 += qDot1 * dt; q1 += qDot2 * dt; q2 += qDot3 * dt; q3 += qDot4 * dt;
        norm = 1.0f / sqrtf(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
        q0 *= norm; q1 *= norm; q2 *= norm; q3 *= norm;
    }
    float getRoll() { return atan2f(2.0f * (q0 * q1 + q2 * q3), 1.0f - 2.0f * (q1 * q1 + q2 * q2)) * 57.2957f; }
    float getPitch() { return asinf(2.0f * (q0 * q2 - q3 * q1)) * 57.2957f; }
    float getYaw() { return atan2f(2.0f * (q0 * q3 + q1 * q2), 1.0f - 2.0f * (q2 * q2 + q3 * q3)) * 57.2957f; }
};

MadgwickFilter ahrs;
float gyroBias[3] = {0, 0, 0};
unsigned long lastUpdate = 0, lastPrint = 0;
float smoothRoll = 0, smoothPitch = 0, smoothYaw = 0;

// HELPER: Write to MPU register
bool writeRegister(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

// HELPER: Read MPU data with error checking to prevent freezing
bool readRawData(int16_t* accel, int16_t* gyro) {
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x3B);
    if (Wire.endTransmission(false) != 0) {
        Serial.println("!!! I2C Bus Hang Detected - Resetting Wire !!!");
        Wire.begin(I2C_SDA, I2C_SCL); // Attempt to recover bus
        return false;
    }
    if (Wire.requestFrom(MPU_ADDR, (uint8_t)14) != 14) return false;
    accel[0] = Wire.read() << 8 | Wire.read();
    accel[1] = Wire.read() << 8 | Wire.read();
    accel[2] = Wire.read() << 8 | Wire.read();
    Wire.read(); Wire.read(); // Skip temp
    gyro[0] = Wire.read() << 8 | Wire.read();
    gyro[1] = Wire.read() << 8 | Wire.read();
    gyro[2] = Wire.read() << 8 | Wire.read();
    return true;
}

void setup() {
    Serial.begin(115200);
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(400000);
    Wire.setTimeOut(50); // IMPORTANT: Prevents code from hanging on I2C errors
    
    pinMode(CLAW_BUTTON_PIN, INPUT_PULLUP);

    WiFi.mode(WIFI_STA);
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE); // Lock Wi-Fi channel for stability

    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }

    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;  
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK) return;

    // MPU9250 Specific Stability Settings
    writeRegister(0x6B, 0x00); // Wake up
    delay(50);
    writeRegister(0x37, 0x02); // Enable Bypass Mode
    writeRegister(0x6A, 0x00); // Disable Master Mode (prevents internal bus collisions)
    
    Serial.println("Calibrating Gyro... Keep sensor still.");
    calibrateGyro();
    Serial.println("System Ready.");
}

void loop() {
    int16_t ra[3], rg[3];
    if (!readRawData(ra, rg)) return;

    unsigned long now = micros();
    float dt = (now - lastUpdate) / 1000000.0f;
    lastUpdate = now;
    if (dt > 0.1f) dt = 0.01f;

    ahrs.updateIMU(rg[0]*(2000.0f/32768.0f)-gyroBias[0], rg[1]*(2000.0f/32768.0f)-gyroBias[1], rg[2]*(2000.0f/32768.0f)-gyroBias[2], ra[0], ra[1], ra[2], dt);

    smoothRoll = smoothRoll * (1.0f - SMOOTHING_ALPHA) + ahrs.getRoll() * SMOOTHING_ALPHA;
    smoothPitch = smoothPitch * (1.0f - SMOOTHING_ALPHA) + ahrs.getPitch() * SMOOTHING_ALPHA;
    smoothYaw = smoothYaw * (1.0f - SMOOTHING_ALPHA) + ahrs.getYaw() * SMOOTHING_ALPHA;

    if (millis() - lastPrint >= 40) { // Send data every 40ms (25Hz) for better stability
        myData.base     = constrain(map((int)smoothYaw,   -70, 70, 0, 180), 0, 180);
        myData.shoulder = constrain(map((int)smoothPitch, -70, 70, 0, 180), 0, 180);
        myData.elbow    = constrain(map((int)smoothRoll,  -70, 70, 0, 180), 0, 180);
        myData.claw     = (digitalRead(CLAW_BUTTON_PIN) == LOW) ? 180 : 0;

        esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));

        // PRINT TO MONITOR FOR DEBUGGING
        Serial.print("Base:"); Serial.print(myData.base);
        Serial.print(" | Shldr:"); Serial.print(myData.shoulder);
        Serial.print(" | Elbow:"); Serial.print(myData.elbow);
        Serial.print(" | Claw:"); Serial.println(myData.claw);

        lastPrint = millis();
    }
}

void calibrateGyro() {
    float sumX=0, sumY=0, sumZ=0;
    for (int i=0; i<CALIBRATION_SAMPLES; i++) {
        int16_t a[3], g[3];
        if(readRawData(a, g)) { sumX += g[0]; sumY += g[1]; sumZ += g[2]; }
        delay(2);
    }
    gyroBias[0] = (sumX/CALIBRATION_SAMPLES) * (2000.0f/32768.0f);
    gyroBias[1] = (sumY/CALIBRATION_SAMPLES) * (2000.0f/32768.0f);
    gyroBias[2] = (sumZ/CALIBRATION_SAMPLES) * (2000.0f/32768.0f);
}