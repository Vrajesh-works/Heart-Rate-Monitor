/*
 * Heart Rate Monitor using ESP32 with BLE
 * includes smoothing, dynamic threshold, and BPM averaging
 * Works with analog pulse sensor connected to GPIO 34
 */

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// BLE UUIDs (unique identifiers)
#define SERVICE_UUID        "0000180d-0000-1000-8000-00805f9b34fb"  // Standard Heart Rate Service
#define CHARACTERISTIC_UUID "00002a37-0000-1000-8000-00805f9b34fb"  // Standard Heart Rate Measurement

BLECharacteristic *pCharacteristic;
bool deviceConnected = false;

#define PULSE_SENSOR_PIN 34  // Pulse sensor signal pin
#define LED_PIN 2            // Built-in LED

// --- Configuration ---
const float SMOOTH_ALPHA = 0.1;         // EMA smoothing factor
const int BASE_THRESHOLD_OFFSET = 150;  // Offset above smoothed signal to detect beats
const int FALLBACK_MARGIN = 100;        // Amount below threshold to reset beat
const int MIN_BEAT_INTERVAL = 300;      // Ignore beats faster than this (ms) (~200 BPM)
const int MAX_BEAT_INTERVAL = 1500;     // Ignore beats slower than this (ms) (~40 BPM)
const int AVG_SIZE = 4;                 // Rolling average window for BPM

// --- Variables ---
float smoothValue = 0;
bool isBeat = false;
unsigned long lastBeatTime = 0;
int bpmArray[AVG_SIZE];
int bpmIndex = 0;
bool firstReading = true;
int currentBPM = 0;

// Detects when device connects/disconnects
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("📱 Device Connected!");
    };
    
    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("📱 Device Disconnected");
      BLEDevice::startAdvertising();  // Let device reconnect
    }
};

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  
  Serial.println("====================================");
  Serial.println("  ESP32 Heart Rate Monitor with BLE");
  Serial.println("====================================");
  delay(1000);
  
  // Initialize BPM array
  for (int i = 0; i < AVG_SIZE; i++) bpmArray[i] = 0;
  
  // Initialize BLE
  Serial.println("Starting BLE...");
  BLEDevice::init("HeartMonitor");
  
  // Create BLE Server
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  
  // Create BLE Service (using standard Heart Rate Service UUID)
  BLEService *pService = pServer->createService(SERVICE_UUID);
  
  // Create BLE Characteristic (using standard Heart Rate Measurement UUID)
  pCharacteristic = pService->createCharacteristic(
                    CHARACTERISTIC_UUID,
                    BLECharacteristic::PROPERTY_READ |
                    BLECharacteristic::PROPERTY_NOTIFY
                  );
  pCharacteristic->addDescriptor(new BLE2902());
  
  // Start service and advertising
  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();
  
  Serial.println("✓ BLE started! Look for 'HeartMonitor'");
  Serial.println();
  Serial.println("Place your finger GENTLY on sensor...");
  Serial.println();
}

void loop() {
  // Read analog pulse sensor
  int sensorValue = analogRead(PULSE_SENSOR_PIN);

  // Initialize smoothing with first value
  if (firstReading) {
    smoothValue = sensorValue;
    firstReading = false;
  }

  // Exponential moving average for noise reduction
  smoothValue = SMOOTH_ALPHA * sensorValue + (1 - SMOOTH_ALPHA) * smoothValue;

  // Dynamic threshold based on smoothed baseline
  int threshold = (int)smoothValue + BASE_THRESHOLD_OFFSET;

  // Detect rising edge — start of heartbeat
  if (sensorValue > threshold && !isBeat) {
    isBeat = true;
    unsigned long currentTime = millis();

    if (lastBeatTime > 0) {
      unsigned long interval = currentTime - lastBeatTime;

      if (interval > MIN_BEAT_INTERVAL && interval < MAX_BEAT_INTERVAL) {
        int bpm = 60000 / interval;

        // Add BPM to rolling average
        bpmArray[bpmIndex] = bpm;
        bpmIndex = (bpmIndex + 1) % AVG_SIZE;

        int total = 0, count = 0;
        for (int i = 0; i < AVG_SIZE; i++) {
          if (bpmArray[i] > 0) { 
            total += bpmArray[i]; 
            count++; 
          }
        }
        int avgBPM = (count > 0) ? total / count : bpm;
        currentBPM = avgBPM;

        Serial.print("❤️ Beat detected!  BPM: ");
        Serial.print(bpm);
        Serial.print(" | Avg BPM: ");
        Serial.println(avgBPM);

        // Send to device via BLE (Standard Heart Rate Service format)
        if (deviceConnected) {
          // Standard Heart Rate Measurement format
          uint8_t hrData[2];
          hrData[0] = 0x00;           // Flags: Heart Rate Value Format is UINT8
          hrData[1] = (uint8_t)avgBPM; // Heart Rate Measurement Value
          
          pCharacteristic->setValue(hrData, 2);
          pCharacteristic->notify();
        }

        // Flash LED
        digitalWrite(LED_PIN, HIGH);
        delay(50);
        digitalWrite(LED_PIN, LOW);
      }
    }

    lastBeatTime = currentTime;
  }

  // Detect falling edge — reset beat state
  if (sensorValue < threshold - FALLBACK_MARGIN) {
    isBeat = false;
  }

  // Timeout — no beat detected recently
  if (millis() - lastBeatTime > 3000 && lastBeatTime != 0) {
    Serial.println("No pulse detected...");
    lastBeatTime = 0; // reset to prevent repeated printing
    currentBPM = 0;
  }

  delay(10);  // Small delay for stable sampling
}