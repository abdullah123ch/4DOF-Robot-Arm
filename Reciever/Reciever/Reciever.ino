#include <esp_now.h>
#include <WiFi.h>
#include <ESP32Servo.h>

// ========== PINS ==========
#define PIN_BASE     13
#define PIN_SHOULDER 12
#define PIN_ELBOW    14
#define PIN_CLAW     27

Servo sBase, sShoulder, sElbow, sClaw;

// ========== SMOOTHING STRUCTURE ==========
struct Joint {
    float current;
    float target;
    float speed;
    uint8_t minLimit;
    uint8_t maxLimit;
};

// Initialize Joints: {Current, Target, Speed, Min, Max}
Joint base     = {90.0, 90.0, 0.5, 10, 170};
Joint shoulder = {90.0, 90.0, 0.4, 0,  180}; // Shoulder is usually slower due to weight
Joint elbow    = {90.0, 90.0, 0.6, 0,  180}; 

typedef struct struct_message {
    uint8_t base;
    uint8_t shoulder;
    uint8_t elbow;
    uint8_t claw;
} struct_message;

struct_message incomingData;
unsigned long lastRecvTime = 0;
const unsigned long TIMEOUT_MS = 1000;

// Update targets when wireless data arrives
void OnDataRecv(const esp_now_recv_info *info, const uint8_t *data, int len) {
    memcpy(&incomingData, data, sizeof(incomingData));
    lastRecvTime = millis();

    if (incomingData.base >= base.minLimit && incomingData.base <= base.maxLimit) 
        base.target = incomingData.base;
        
    shoulder.target = incomingData.shoulder;
    elbow.target    = incomingData.elbow;
    sClaw.write(incomingData.claw); // Claw usually stays instant for "grip" feel
  
    Serial.print("RX -> B:"); Serial.print(incomingData.base);
    Serial.print(" S:"); Serial.print(incomingData.shoulder);
    Serial.print(" E:"); Serial.print(incomingData.elbow);
    Serial.print(" G:"); Serial.println(incomingData.claw);
}

void setup() {
    Serial.begin(115200);
    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    
    sBase.attach(PIN_BASE);
    sShoulder.attach(PIN_SHOULDER);
    sElbow.attach(PIN_ELBOW);
    sClaw.attach(PIN_CLAW);

    // Initial Positions
    sBase.write(base.current);
    sShoulder.write(shoulder.current);
    sElbow.write(elbow.current);

    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) return;
    esp_now_register_recv_cb(OnDataRecv);
}

void loop() {
    // --- SMOOTHING LOGIC FOR ALL JOINTS ---
    updateJoint(sBase, base);
    updateJoint(sShoulder, shoulder);
    updateJoint(sElbow, elbow);

    // --- SERIAL DEBUG / MANUAL ---
    if (Serial.available() > 0) {
        char cmd = Serial.read();
        int angle = Serial.parseInt();
        if (angle >= 0 && angle <= 180) {
            lastRecvTime = millis();
            if (cmd == 'B' || cmd == 'b') base.target = angle;
            if (cmd == 'S' || cmd == 's') shoulder.target = angle;
            if (cmd == 'E' || cmd == 'e') elbow.target = angle;
            if (cmd == 'C' || cmd == 'c') sClaw.write(angle);
        }
    }

    // --- FAILSAFE ---
    if (millis() - lastRecvTime > TIMEOUT_MS) {
        base.target = 90; shoulder.target = 90; elbow.target = 90; sClaw.write(0);
    }
    
    delay(15); 
}

// Function to incrementally move any joint toward its target
void updateJoint(Servo &s, Joint &j) {
    if (abs(j.current - j.target) > 0.5) {
        if (j.current < j.target) j.current += j.speed;
        else j.current -= j.speed;
        s.write((int)j.current);
    }
}