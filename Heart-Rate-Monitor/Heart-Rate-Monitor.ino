/*
 * Simple Heart Rate Monitor with ESP32
 * Reads pulse sensor and displays BPM in Serial Monitor
 */

#define PULSE_SENSOR_PIN 34  // Pulse sensor connected to GPIO 34

int sensorValue = 0;
int threshold = 2000;         // Adjust this value if needed
int BPM = 0;
unsigned long lastBeatTime = 0;
bool isBeat = false;

void setup() {
  Serial.begin(115200);
  Serial.println("Heart Rate Monitor Ready");
}

void loop() {
  // Read pulse sensor
  sensorValue = analogRead(PULSE_SENSOR_PIN);
  
  // Detect heartbeat
  if (sensorValue > threshold && !isBeat) {
    isBeat = true;
    unsigned long currentTime = millis();
    
    // Calculate BPM
    if (lastBeatTime != 0) {
      unsigned long timeBetweenBeats = currentTime - lastBeatTime;
      BPM = 60000 / timeBetweenBeats;  // Convert to beats per minute
      
      // Display only valid heart rates
      if (BPM > 40 && BPM < 200) {
        Serial.print("BPM: ");
        Serial.println(BPM);
      }
    }
    lastBeatTime = currentTime;
  }
  
  // Reset for next beat
  if (sensorValue < threshold) {
    isBeat = false;
  }
  
  delay(20);
}