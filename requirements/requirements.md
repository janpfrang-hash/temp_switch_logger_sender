# Requirements — LoRa Door/Environment Sensor System
 
**Status:** Draft v0.2 (supersedes `ReQ_loRa.txt`)
**Hardware baseline:** Heltec WiFi LoRa 32 V4 (ESP32-S3R2 + SX1262) for both devices
**Related document:** `LoRa_Door_Sensor_Architecture.md`
 
---
 
## 1. Overview
 
Two LoRa-connected devices plus a backend platform:
 
- **Device 1 ("Battery")** — battery-powered sensor node, deployed at the door.
- **Device 2 ("Home")** — mains-powered gateway, always on.
- **Data Platform** — receives, stores, and forwards data/alarms to a mobile app.
---
 
## 2. Device 1 — "Battery" sensor node
 
| # | Requirement |
|---|---|
| R1 | Powered at 3.3 V. Either via LiPo battery (on-board charge management) **or** directly via the board's `3V3` header pins (Header J3, pins 2/3 — a documented supply mode, 2.7–3.5 V / ≥150 mA). Only one supply source connected at a time. |
| R2 | Maximize battery/idle life: main CPU stays in **deep sleep** between events; wakes only via RTC timer or GPIO interrupt (`ext1`). *(Clarified: the ESP32-S3 ULP coprocessor is not used — every wake cycle ends in a LoRa transmission over SPI, which the ULP cannot perform, so the main CPU has to wake regardless. Deep sleep alone already reaches the datasheet's ~20 µA sleep current.)* |
| R3 | OLED display disabled/unpowered by default to save energy. |
| R4 | Reed switch (door sensor): a state change **immediately** wakes the main CPU (not waiting for the next scheduled cycle) and transmits a door-alarm LoRa message right away, then returns to deep sleep. |
| R5 | Read BMP280 (temperature + pressure) every 15 minutes and transmit via LoRa. |
| R6 | Read DS18B20 (OneWire temperature, 3-wire) every 15 minutes and transmit via LoRa. |
| R7 | *(Future / not in current scope)* Add a low-energy eInk display for local readout. Firmware should keep a `display` module interface generic enough to add this later without restructuring. |
 
---
 
## 3. Device 2 — "Home" gateway
 
| # | Requirement |
|---|---|
| R8 | Mains-powered (USB-C / 5 V), always on — no sleep. |
| R9 | Receive LoRa messages from Device 1 (sensor readings and door alarms). |
| R10 | Forward received data to the Data Platform over the internet (transport/platform TBD — e.g. MQTT or HTTPS), in a form usable by a mobile app. |
 
---
 
## 4. Data Platform
 
| # | Requirement |
|---|---|
| R11 | Receive data forwarded by Device 2. |
| R12 | Process and store the data (sensor readings, alarm events). |
| R13 | Make the data accessible to a mobile app (current status + history). |
| R14 | Push status messages and alarms to a user's mobile device (e.g., "door open", "temperature is XXX"). |
 
*(Concrete platform choice is intentionally open — see architecture doc, Section 5.)*
 
---
 
## 5. Design Decisions Clarified During Architecture Review
 
These aren't new requirements but constraints/decisions that resolve ambiguity in R1–R7 and should guide implementation:
 
- **Power:** Both a LiPo-powered and a direct-3.3V-powered deployment are supported by the hardware; firmware should not assume a LiPo is always present (e.g., skip/guard battery-voltage telemetry if no LiPo is connected, since that ADC reads the LiPo net specifically, not the general 3.3 V rail).
- **Sleep strategy:** Plain ESP32-S3 deep sleep (RTC timer wake + `ext1` GPIO wake), no ULP coprocessor involvement.
- **Configuration:** Firmware config kept in a single `config.h` (all pins, LoRa parameters, intervals, role selection) plus a separate `secrets.h` for credentials (excluded from version control).
- **Toolchain:** Plain Arduino IDE sketch (no PlatformIO), built into binaries via GitHub Actions, flashed with an existing Python-based flashing tool.
