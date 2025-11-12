/*
 * Heart Rate Monitor with Bluetooth (BLE)
 * ESP32 sends BPM data via Bluetooth Low Energy
 * Compatible with heart rate monitor apps and devices
 * Pulse sensor on GPIO 34
 */

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define PULSE_SENSOR_PIN 34

// BLE Heart Rate Service (standard UUID)
#define SERVICE_UUID        "180D"  // Heart Rate Service
#define CHARACTERISTIC_UUID "2A37"  // Heart Rate Measurement

// --- Configuration ---
const float SMOOTH_ALPHA = 0.1;
const int BASE_THRESHOLD_OFFSET = 150;
const int FALLBACK_MARGIN = 100;
const int MIN_BEAT_INTERVAL = 300;
const int MAX_BEAT_INTERVAL = 1500;
const int AVG_SIZE = 4;

// --- Variables ---
float smoothValue = 0;
bool isBeat = false;
unsigned long lastBeatTime = 0;
int bpmArray[AVG_SIZE];
int bpmIndex = 0;
bool firstReading = true;
int currentBPM = 0;

// BLE Variables
BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

// BLE Server Callbacks
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("📱 Device connected!");
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("📱 Device disconnected");
    }
};

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Heart Rate Monitor with BLE");
  
  // Initialize BPM array
  for (int i = 0; i < AVG_SIZE; i++) bpmArray[i] = 0;
  
  // --- BLE Setup ---
  BLEDevice::init("ESP32_HR_Monitor");
  
  // Create BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  
  // Create Heart Rate Service
  BLEService *pService = pServer->createService(SERVICE_UUID);
  
  // Create Heart Rate Measurement Characteristic
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  
  // Add descriptor for notifications
  pCharacteristic->addDescriptor(new BLE2902());
  
  // Start service
  pService->start();
  
  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  
  Serial.println("✅ BLE Heart Rate Service started!");
  Serial.println("Waiting for connection...");
  
  delay(1000);
}

void loop() {
  // Read and process pulse sensor
  int sensorValue = analogRead(PULSE_SENSOR_PIN);
  
  // Initialize smoothing
  if (firstReading) {
    smoothValue = sensorValue;
    firstReading = false;
  }
  
  // EMA smoothing
  smoothValue = SMOOTH_ALPHA * sensorValue + (1 - SMOOTH_ALPHA) * smoothValue;
  
  // Dynamic threshold
  int threshold = (int)smoothValue + BASE_THRESHOLD_OFFSET;
  
  // Detect beat
  if (sensorValue > threshold && !isBeat) {
    isBeat = true;
    unsigned long currentTime = millis();
    
    if (lastBeatTime > 0) {
      unsigned long interval = currentTime - lastBeatTime;
      
      if (interval > MIN_BEAT_INTERVAL && interval < MAX_BEAT_INTERVAL) {
        int bpm = 60000 / interval;
        
        // Rolling average
        bpmArray[bpmIndex] = bpm;
        bpmIndex = (bpmIndex + 1) % AVG_SIZE;
        
        int total = 0, count = 0;
        for (int i = 0; i < AVG_SIZE; i++) {
          if (bpmArray[i] > 0) { 
            total += bpmArray[i]; 
            count++; 
          }
        }
        currentBPM = (count > 0) ? total / count : bpm;
        
        Serial.print("❤️ Beat!  BPM: ");
        Serial.print(bpm);
        Serial.print(" | Avg: ");
        Serial.print(currentBPM);
        Serial.println(deviceConnected ? " | 📡 Sent via BLE" : "");
        
        // Send BPM via BLE if connected
        if (deviceConnected) {
          sendHeartRateBLE(currentBPM);
        }
      }
    }
    
    lastBeatTime = currentTime;
  }
  
  // Reset beat state
  if (sensorValue < threshold - FALLBACK_MARGIN) {
    isBeat = false;
  }
  
  // Connection state management
  if (!deviceConnected && oldDeviceConnected) {
    delay(500);
    pServer->startAdvertising();
    Serial.println("🔄 Restarting advertising...");
    oldDeviceConnected = deviceConnected;
  }
  
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }
  
  delay(5);
}

// Send heart rate data via BLE (standard format)
void sendHeartRateBLE(int bpm) {
  // Heart Rate Measurement format (BLE standard)
  // Byte 0: Flags (0x00 = uint8 BPM format)
  // Byte 1: BPM value
  uint8_t hrData[2];
  hrData[0] = 0x00;  // Flags: Heart Rate Value Format (uint8)
  hrData[1] = (uint8_t)bpm;
  
  pCharacteristic->setValue(hrData, 2);
  pCharacteristic->notify();
}