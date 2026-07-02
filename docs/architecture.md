# System Architecture — LoRa Door/Environment Sensor System

**Status:** Draft v0.1
**Scope:** Device 1 (battery sensor node), Device 2 (mains-powered gateway), Data Platform
**Hardware baseline:** Heltec **WiFi LoRa 32 V4** (ESP32‑S3R2 + SX1262), 0.96" OLED, LiPo management, USB‑C
**Toolchain:** Arduino IDE, binaries built via GitHub Actions, flashed with existing Python flashing tool

---

## 1. Purpose & Requirements Summary

Derived from `ReQ_loRa.txt`:

| # | Requirement | Device |
|---|---|---|
| R1 | Battery-powered (3.3 V), maximize ULP / deep-sleep time | Device 1 |
| R2 | Display shut off by default (OLED disabled to save power) | Device 1 |
| R3 | Detect reed switch (door) state change → immediate LoRa alarm | Device 1 |
| R4 | Read BMP280 (temperature + pressure) every 15 min → LoRa | Device 1 |
| R5 | Read DS18B20 (OneWire temperature) every 15 min → LoRa | Device 1 |
| R6 | Future: low-energy eInk display for local readout | Device 1 (later) |
| R7 | Mains-powered, always-on receiver | Device 2 |
| R8 | Receive LoRa packets from Device 1 | Device 2 |
| R9 | Forward data to internet platform, reachable from a mobile app | Device 2 |
| R10 | Platform receives/processes/stores data, pushes alarms & status to mobile | Data Platform |

---

## 2. System Overview

```
 ┌───────────────────────┐        LoRa (868 MHz)        ┌───────────────────────┐
 │       DEVICE 1         │ ───────────────────────────▶ │       DEVICE 2         │
 │  "Battery" sensor node │                               │   "Home" gateway       │
 │  ESP32-S3 + SX1262     │ ◀─── (optional ACK/config) ── │  ESP32-S3 + SX1262     │
 │  Reed / BMP280 / DS18B20│                              │  WiFi uplink           │
 └───────────────────────┘                               └───────────┬───────────┘
                                                                       │ MQTT/HTTPS
                                                                       ▼
                                                          ┌───────────────────────┐
                                                          │     Data Platform      │
                                                          │  ingest → store →      │
                                                          │  process → notify      │
                                                          └───────────┬───────────┘
                                                                       │ push/API
                                                                       ▼
                                                              ┌─────────────────┐
                                                              │   Mobile App /   │
                                                              │  Notifications    │
                                                              └─────────────────┘
```

Device 1 and Device 2 run the **same base firmware** (shared LoRa/config/protocol modules), differentiated by a build-time `ROLE_NODE` / `ROLE_GATEWAY` flag, so common code isn't duplicated.

---

## 3. Hardware Reference

Board: **Heltec WiFi LoRa 32 V4** — ESP32-S3R2 (Wi-Fi/BLE, 2 MB PSRAM, 16 MB Flash), SX1262 LoRa transceiver, SSD1315 OLED (I²C), integrated LiPo charge/management, solar input, USB‑C.

### 3.1 Fixed on-board wiring (from schematic, not reassignable)

| Function | GPIO | Notes |
|---|---|---|
| LoRa NSS (CS) | GPIO8 | SX1262 SPI |
| LoRa SCK | GPIO9 | SX1262 SPI |
| LoRa MOSI | GPIO10 | SX1262 SPI |
| LoRa MISO | GPIO11 | SX1262 SPI |
| LoRa RESET | GPIO12 | SX1262 |
| LoRa BUSY | GPIO13 | SX1262 |
| LoRa DIO1 (IRQ) | GPIO14 | SX1262 TX/RX-done IRQ |
| OLED SDA | GPIO17 | I²C, SSD1315 |
| OLED SCL | GPIO18 | I²C, SSD1315 |
| OLED RESET | GPIO21 | |
| Vext control | GPIO36 | Drive **HIGH** to enable the `Ve` 3.3 V rail (powers external peripherals / OLED) |
| Status LED | GPIO35 | On-board LED |
| PRG / User button | GPIO0 | Also boot-mode select |
| Battery ADC enable | GPIO37 | Drive HIGH before sampling GPIO1 |
| Battery voltage read | GPIO1 (ADC1_CH0) | `VBAT = 100/(100+390) * V(GPIO1)` |
| Reset button | RST / CHIP_PU | Hardware reset |

### 3.2 Proposed external sensor wiring (free header pins, Device 1 only)

| Sensor | Signal | GPIO (Header J3) | Rationale |
|---|---|---|---|
| BMP280 | I²C SDA/SCL | **GPIO17 / GPIO18 (shared OLED bus)** | BMP280 (0x76/0x77) coexists with SSD1315 (0x3C) on one I²C bus — no extra pins needed |
| DS18B20 | OneWire data | **GPIO4** | Free RTC-capable GPIO, external 4.7 kΩ pull-up to 3V3 |
| Reed switch (door) | Digital in, wake source | **GPIO3** | Free RTC GPIO → usable as `ext1` deep-sleep wake pin; external pull-up, switch to GND |

> Both sensors and the reed switch should be powered from the **`Ve` rail (GPIO36/Vext_Ctrl)** where possible, so they are fully powered down between wake cycles. The reed switch itself is passive and doesn't need power.

### 3.3 Power supply options / direct 3.3 V power

The board supports several independent supply modes (datasheet Table 3.2); the two relevant ones here:

| Mode | Pin | Min / Typ / Max |
|---|---|---|
| LiPo battery (via on-board charger) | `JP2` battery connector | 3.3 / 3.7 / 4.2 V |
| **Direct 3.3 V supply** | **`3V3` pins, Header J3 (pins 2 & 3)** | **2.7 / 3.3 / 3.5 V, ≥150 mA** |

**Yes — the board can be powered directly at 3.3 V** via the J3 `3V3` pins; this is a documented supply mode, not a misuse of the regulator output pin. Implications for Device 1:

- If Device 1 is powered this way (external 3.3 V source instead of the LiPo), **do not** also connect USB or the LiPo/solar inputs at the same time — the `3V3` pin bypasses the on-board CE6260B33M regulators and connects straight to the `VDD_3V3` rail.
- The battery-voltage read circuit (`GPIO1`/`ADC1_CH0` via the `VBAT_Read` divider) is wired to the **LiPo `VBAT` net**, not to `VDD_3V3`. If no LiPo is connected, this reading is meaningless — drop the battery-voltage telemetry (or repurpose the ADC to monitor the external 3.3 V rail directly through a suitable divider) for this configuration.
- The on-board charge management (CN3165) and solar input become irrelevant in this configuration and can be left unpopulated/unconnected.

### 3.4 Power topology (Device 1)

- Supplied either via LiPo (on-board charger) or directly via the J3 `3V3` pins (Section 3.3) — pick one per deployment.
- ESP32-S3 **deep sleep** (main CPU off) between events; RTC timer + `ext1` GPIO wake (reed switch) are the only wake sources. See Section 4.3 — the ULP coprocessor is not used (not needed).
- OLED and any non-essential peripherals are held in reset / unpowered (`Vext_Ctrl = LOW`) except during the brief measurement/transmit window.

### 3.5 Power topology (Device 2)

- USB-C / 5 V powered, no sleep — Wi-Fi + LoRa receiver always active.
- OLED optionally used for local status (link quality, last message).

---

## 4. Firmware Architecture (Arduino IDE / GitHub project)

### 4.1 Design principles

- **One repository, one shared core**, split into reusable modules; role-specific code isolated behind a compile-time switch.
- **Config separated from logic** — all pins, timing intervals, LoRa parameters, and credentials live in header/config files, never hard-coded in application logic.
- Hardware access (LoRa, sensors, power) wrapped in thin driver modules with a small interface, so the main loop / state machine doesn't touch registers directly.
- Sleep-safe: all persistent state needed across a deep-sleep cycle stored in **RTC memory** (`RTC_DATA_ATTR`), not regular RAM.

### 4.2 Repository / folder structure (Arduino IDE, no PlatformIO)

Build target: **plain Arduino IDE sketch**, compiled by **GitHub Actions** (`arduino-cli` or the `arduino/compile-sketches` action) into binaries, which are then flashed with your existing Python flashing tool (e.g. `esptool.py`-based). No `platformio.ini`.

The Arduino build system compiles every `.ino`/`.cpp`/`.h` that sits **directly in the sketch folder** (flat, using IDE "tabs") — subfolders are not compiled automatically, with one exception: a folder literally named `src/` inside the sketch directory *is* compiled recursively by `arduino-cli`/Arduino IDE ≥1.6.6, which we use to keep the modularity without cluttering the sketch root:

```
lora-door-sensor/
├── README.md
├── docs/
│   └── architecture.md              # this document
├── .github/
│   └── workflows/
│       └── build.yml                # arduino-cli build → artifact (.bin) for the flashing tool
└── LoraDoorSensor/                  # the Arduino sketch (folder name == main .ino name)
    ├── LoraDoorSensor.ino           # setup()/loop(), role dispatch (thin)
    └── src/                        # compiled recursively by arduino-cli/IDE
        ├── config/
        │   ├── config.h               # ALL user-editable board/app config: GPIO pins,
        │   │                          # LoRa params, intervals/thresholds, ROLE_NODE/ROLE_GATEWAY
        │   └── secrets.h               # WiFi/MQTT credentials (gitignored; secrets.h.example committed)
        ├── lora/
        │   ├── lora_driver.h/.cpp    # SX1262 init, send, receive, sleep
        │   └── lora_protocol.h/.cpp  # packet struct, encode/decode, CRC, message types
        ├── power/
        │   ├── power_mgmt.h/.cpp     # deep sleep entry/exit, wake-reason handling, Vext control
        │   └── battery.h/.cpp        # battery ADC read
        ├── sensors/
        │   ├── sensor_bmp280.h/.cpp
        │   ├── sensor_ds18b20.h/.cpp
        │   └── sensor_reed.h/.cpp
        ├── display/
        │   └── oled_display.h/.cpp   # disabled by default; stub for future eInk driver
        ├── node/
        │   └── node_app.h/.cpp       # Device 1 state machine (R1–R6)
        ├── gateway/
        │   └── gateway_app.h/.cpp    # Device 2: receive, WiFi/MQTT uplink (R7–R9)
        └── net/
            └── uplink.h/.cpp         # WiFi connect + MQTT/HTTPS client (gateway only)
```

Notes:
- `ROLE_NODE` vs `ROLE_GATEWAY` is a `#define` in `config.h` (or a build flag passed via `arduino-cli --build-property` in the CI matrix), so the same sketch builds two different binaries — one per device.
- Third-party libraries (SX1262 driver, e.g. RadioLib; sensor libraries for BMP280/DS18B20/OneWire) are declared in the CI workflow (`arduino-cli lib install ...`) rather than vendored, keeping the repo clean.
- If you'd rather avoid the `src/` subfolder convention (it's supported but occasionally surprises people migrating between IDE versions), the fallback is fully flat: all `.h`/`.cpp` files directly under `LoraDoorSensor/` as IDE tabs, same content, no subfolders. Functionally equivalent — the `src/` layout is just easier to navigate in a code editor / on GitHub.

### 4.3 Device 1 ("node") state machine

> **Note on "ULP mode":** the ESP32-S3's ULP-RISC-V coprocessor can run small tasks (ADC sampling, simple GPIO/RTC-I2C logic) while the main CPU stays powered off. It is **not used here**, because it cannot drive the SX1262 over SPI — and every wake cycle in this design (15‑min telemetry *and* the door alarm) ends in a LoRa transmission, so the main CPU has to wake up regardless of how the sensors are read. Using ULP would add complexity without a power-saving benefit in this specific use case.
> Instead, Device 1 relies on plain **deep sleep** (main CPU fully powered down): the datasheet's deep-sleep current (≈20 µA, battery or 3.3 V-header powered — Table 3.4) is reached without ULP, using only:
> - **RTC timer wake** for the 15-minute measurement cycle, and
> - **`ext1` GPIO wake** on the reed-switch pin (`GPIO3`, RTC-capable) for the immediate door alarm.
>
> ULP would only become useful if you later wanted the coprocessor to *pre-filter* many samples and wake the CPU only occasionally without transmitting every time — not applicable to the current requirements (R3–R5 all require a report on every event/interval).

```
                ┌─────────────┐
     power-on / │  BOOT /     │
     timer wake │  WAKE       │◀───────────────┐
                └──────┬──────┘                │
                       │ read wake reason        │
                       ▼                         │
             ┌────────────────────┐              │
             │ TIMER wake (15 min)│  REED wake   │
             │ → read BMP280 +    │  → read reed │
             │   DS18B20          │    state     │
             └─────────┬──────────┘              │
                       ▼                         │
             ┌────────────────────┐              │
             │ Build LoRa packet  │              │
             │ (sensor or alarm)  │              │
             └─────────┬──────────┘              │
                       ▼                         │
             ┌────────────────────┐              │
             │ TX via SX1262      │              │
             │ (short RX window   │              │
             │  for optional ACK) │              │
             └─────────┬──────────┘              │
                       ▼                         │
             ┌────────────────────┐              │
             │ Power down          │             │
             │ peripherals,        │             │
             │ configure ext1 wake │             │
             │ (reed) + timer wake │             │
             │ → deep sleep ───────┴─────────────┘
             └────────────────────┘
```

- **Reed-switch wake, confirmed:** yes — the `REED wake` branch in the diagram above already covers this. When the door reed switch toggles, `ext1` wakes the main CPU from deep sleep (not just the ULP — the full CPU, since a LoRa TX needs it), the firmware reads the current door state, builds a `msg_type=0x02` (door alarm) packet, and transmits it via the SX1262 **immediately**, before going back to sleep. This path is fully independent of the 15‑minute timer path — it fires the moment the switch changes, not on the next scheduled cycle.
- Reed-switch state changes trigger an **immediate** wake + alarm packet (R3), independent of the 15-minute measurement cycle (R4/R5).
- The two events (timer, reed) can be merged into a single transmission if they occur within the same wake window, to conserve airtime/energy.
- OLED stays powered off (R2); only enabled if a future debug/config mode is explicitly entered (long button press on GPIO0).

### 4.4 Device 2 ("gateway") application

- Runs continuously: SX1262 in RX mode, ESP32-S3 Wi-Fi connected.
- On packet receipt: validate CRC/message type → push to an internal queue → publish via MQTT (or HTTPS POST) to the Data Platform → optionally send LoRa ACK back to the node.
- OLED (optional) shows last-seen node, RSSI/SNR, and link status.
- Should implement basic buffering/retry if the internet uplink is temporarily down (e.g., local queue in RAM/flash).

### 4.5 LoRa message protocol (`lora_protocol`)

Compact binary format to minimize airtime/energy on Device 1:

| Field | Size | Description |
|---|---|---|
| `node_id` | 1 B | Sender identifier |
| `msg_type` | 1 B | `0x01`=periodic sensor, `0x02`=door alarm, `0x03`=heartbeat/battery |
| `seq` | 1 B | Rolling sequence number (duplicate/loss detection) |
| `battery_mV` | 2 B | Battery voltage |
| `payload` | variable | Type-specific: `{temp_bmp, pressure}` / `{door_state}` / `{temp_ds18b20}` |
| `crc16` | 2 B | Integrity check |

Keeping payload variants under one envelope lets both `node_app` and `gateway_app` share a single encode/decode implementation (`lora_protocol.cpp`), satisfying the "shared core" design goal.

### 4.6 Configuration layer

Kept to **two files**, which is the right amount of separation for a project this size:

- `config.h`: single source of truth for everything that isn't a secret — all GPIO pin assignments (Section 3), LoRa region/frequency/spreading-factor/bandwidth, measurement interval (default 15 min), sleep durations, node ID, and role (`ROLE_NODE`/`ROLE_GATEWAY`). A hardware revision or pin change means editing one file.
- `secrets.h`: Wi-Fi SSID/password, MQTT broker/token — kept separate on purpose (excluded via `.gitignore`; `secrets.h.example` committed as a template), since it's the one file that must never end up in the GitHub repo.

Splitting pins into their own `pins.h` only pays off once you're targeting multiple board revisions/variants with genuinely different wiring; with a single board (Heltec V4) and two roles distinguished by a `#define`, one `config.h` is simpler to navigate and there's nothing to keep in sync between two files.

---

## 5. Data Platform (outline, to be detailed separately)

- **Ingest:** MQTT broker or HTTPS webhook receiving Device 2 uplinks.
- **Store:** time-series storage for sensor readings; simple event log for alarms.
- **Process:** threshold checks (temperature range, door-open duration), data aggregation for the mobile app.
- **Notify:** push notification service (e.g., via a mobile backend / notification provider) triggered on alarms or threshold breaches.
- **API:** read endpoint for the mobile app to fetch current status/history.

*(Concrete platform choice — self-hosted MQTT+DB+backend vs. managed IoT service — is intentionally left open pending a separate decision.)*

---

## 6. Open Points / Future Work

- Confirm LoRa regional parameters (frequency plan, duty cycle limits) for the deployment region.
- Define exact deep-sleep current budget vs. LiPo capacity to estimate battery life.
- Design eInk display module (R6) behind the same `display/` interface currently stubbed for OLED.
- Decide on ACK/retry strategy for reliability of alarm messages (R3 is safety-relevant).
- Choose and document the concrete Data Platform stack (Section 5).
