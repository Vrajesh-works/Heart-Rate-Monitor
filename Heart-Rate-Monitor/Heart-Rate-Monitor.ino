/*
 * Heart Rate Monitor using ESP32
 * No external libraries — includes smoothing, dynamic threshold, and BPM averaging
 * Works with analog pulse sensor connected to GPIO 34
 */

#define PULSE_SENSOR_PIN 34  // Pulse sensor signal pin

// --- Configuration ---
const float SMOOTH_ALPHA = 0.1;     // EMA smoothing factor
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

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Heart Rate Monitor (No Library)");
  delay(1000);
  for (int i = 0; i < AVG_SIZE; i++) bpmArray[i] = 0;
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
          if (bpmArray[i] > 0) { total += bpmArray[i]; count++; }
        }
        int avgBPM = (count > 0) ? total / count : bpm;

        Serial.print("❤️ Beat detected!  BPM: ");
        Serial.print(bpm);
        Serial.print(" | Avg BPM: ");
        Serial.println(avgBPM);
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
  }

  delay(5);  // Small delay for stable sampling
}
