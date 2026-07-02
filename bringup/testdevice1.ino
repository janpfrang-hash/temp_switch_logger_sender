#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <RadioLib.h>
#include <Adafruit_BMP280.h>
#include "driver/rtc_io.h"

// ==========================================
// PIN DEFINITIONS (Heltec V4 - ESP32-S3)
// ==========================================
// SX1262 LoRa Pins
#define LORA_NSS    8
#define LORA_SCK    9
#define LORA_MOSI   10
#define LORA_MISO   11
#define LORA_RST    12
#define LORA_BUSY   13
#define LORA_DIO1   14

// I2C Pins for BMP280
#define I2C_SDA     4
#define I2C_SCL     5

// Reed Switch Pin (Must be an RTC GPIO, ESP32-S3 supports 0-21)
#define REED_PIN    2

// ==========================================
// SETTINGS
// ==========================================
#define SLEEP_INTERVAL_MIN 15
#define uS_TO_S_FACTOR 1000000ULL

// ==========================================
// INSTANCES & RTC MEMORY
// ==========================================
SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY);
Adafruit_BMP280 bmp;

// These variables survive deep sleep
RTC_DATA_ATTR float lastTemp = 0.0;
RTC_DATA_ATTR float lastPressure = 0.0;
RTC_DATA_ATTR int bootCount = 0;

void setup() {
  Serial.begin(115200);
  delay(1000); // Give serial monitor time to open
  
  bootCount++;
  Serial.printf("\n--- Boot Count: %d ---\n", bootCount);

  // 1. Determine Wakeup Reason
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  String wakeReasonStr = "Unknown/PowerOn";
  bool readBMP = true; // Default to true on fresh boot

  if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
    wakeReasonStr = "Timer (15 Min)";
    readBMP = true; // 15 mins passed, read the sensor
  } 
  else if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
    wakeReasonStr = "Reed Switch Changed";
    readBMP = false; // Just use last known BMP values to save time/battery
  }

  Serial.println("Wakeup cause: " + wakeReasonStr);

  // 2. Read BMP280 if necessary
  if (readBMP) {
    Wire.begin(I2C_SDA, I2C_SCL);
    if (bmp.begin(0x76) || bmp.begin(0x77)) { // Standard I2C addresses
      bmp.setSampling(Adafruit_BMP280::MODE_FORCED,     
                      Adafruit_BMP280::SAMPLING_X1,     
                      Adafruit_BMP280::SAMPLING_X1,    
                      Adafruit_BMP280::FILTER_OFF);     
      
      bmp.takeForcedMeasurement(); // Only power on sensor briefly
      lastTemp = bmp.readTemperature();
      lastPressure = bmp.readPressure() / 100.0F; // Convert to hPa
      Serial.printf("BMP280 Updated - Temp: %.2f C, Press: %.2f hPa\n", lastTemp, lastPressure);
    } else {
      Serial.println("BMP280 not found! Check wiring.");
    }
  }

  // 3. Read current Reed Switch state
  pinMode(REED_PIN, INPUT_PULLUP);
  int reedState = digitalRead(REED_PIN); // 0 = Closed (Magnet nearby), 1 = Open
  Serial.printf("Current Reed State: %s\n", reedState == 0 ? "CLOSED" : "OPEN");

  // 4. Transmit data via LoRa
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
  
  // Initialize LoRa with standard EU868 settings (adjust frequency for your region)
  Serial.print("Initializing LoRa... ");
  int state = radio.begin(868.0, 125.0, 9, 7, 18, 10); 
  
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("Success!");
    
    // Construct payload
    String payload = "{\"reason\":\"" + wakeReasonStr + "\",";
    payload += "\"temp\":" + String(lastTemp) + ",";
    payload += "\"press\":" + String(lastPressure) + ",";
    payload += "\"reed\":" + String(reedState) + "}";

    Serial.println("Transmitting: " + payload);
    radio.transmit(payload);
    
    // Put LoRa chip to sleep to save power
    radio.sleep();
  } else {
    Serial.printf("Failed, code %d\n", state);
  }

  // 5. Configure Deep Sleep Requirements
  
  // Set Timer Wakeup
  esp_sleep_enable_timer_wakeup(SLEEP_INTERVAL_MIN * 60 * uS_TO_S_FACTOR);
  
  // Set Reed Switch Wakeup to the OPPOSITE of the current state
  // This achieves the "wake on change" functionality.
  rtc_gpio_pullup_en((gpio_num_t)REED_PIN);   // Ensure pullup stays on during deep sleep
  rtc_gpio_pulldown_dis((gpio_num_t)REED_PIN);
  
  int wakeOnLevel = (reedState == 0) ? 1 : 0; 
  esp_sleep_enable_ext0_wakeup((gpio_num_t)REED_PIN, wakeOnLevel);
  
  Serial.printf("Going to deep sleep. Will wake on Timer OR if Reed pin goes to level %d.\n", wakeOnLevel);
  Serial.flush();
  
  // 6. Go to Sleep
  esp_deep_sleep_start();
}

void loop() {
  // Never reached, the ESP32 wakes up and starts from setup() again.
}
