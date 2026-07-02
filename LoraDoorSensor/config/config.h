/**
 * config.h
 * -----------------------------------------------------------------------
 * Single source of truth for Device 1 ("Battery" node) — LoRa Door /
 * Environment Sensor System.
 *
 * Board:    Heltec WiFi LoRa 32 V4 (ESP32-S3R2 + SX1262)
 * Role:     ROLE_NODE (battery sensor node, see ReQ_loRa.md R1-R7)
 *
 * Per architecture.md Section 4.6: this file holds everything that is
 * NOT a secret (pins, bus config, LoRa radio params, timing, node id).
 * WiFi/MQTT credentials belong in secrets.h (gitignored), not here.
 *
 * To build Device 2 ("gateway") from the same sketch, this file is
 * swapped/duplicated with ROLE_GATEWAY defined instead — see the
 * ROLE_NODE / ROLE_GATEWAY switch below.
 * -----------------------------------------------------------------------
 */

#pragma once

// =========================================================================
// 1. ROLE SELECTION
// =========================================================================
// This build is Device 1. Do not define ROLE_GATEWAY in the same build.
#define ROLE_NODE
// #define ROLE_GATEWAY   // <-- Device 2 uses this instead, in its own config.h

#define NODE_ID            0x01      // 1 B, unique per physical node on this network

// =========================================================================
// 2. FIXED ON-BOARD PINS (Heltec WiFi LoRa 32 V4 — not reassignable,
//    see LoRa_Door_Sensor_Architecture.md Section 3.1)
// =========================================================================

// --- SX1262 LoRa radio (SPI) ---
#define PIN_LORA_NSS        8        // SPI CS
#define PIN_LORA_SCK         9        // SPI SCK
#define PIN_LORA_MOSI        10       // SPI MOSI
#define PIN_LORA_MISO        11       // SPI MISO
#define PIN_LORA_RESET        12       // SX1262 RESET
#define PIN_LORA_BUSY        13       // SX1262 BUSY
#define PIN_LORA_DIO1        14       // SX1262 DIO1 (TX/RX-done IRQ)
// SX1262 has no dedicated DIO2 (RF switch) or DIO3 wired out on this board;
// RadioLib SX1262 constructor: SX1262(nss, dio1, reset, busy)

// --- OLED (SSD1315, I2C) — kept off by default (R2/R3), interface reserved ---
#define PIN_OLED_SDA        17
#define PIN_OLED_SCL        18
#define PIN_OLED_RST        21
#define OLED_I2C_ADDR        0x3C

// --- Power / board control ---
#define PIN_VEXT_CTRL        36       // Drive HIGH to enable Ve (3V3) rail to peripherals
#define VEXT_ACTIVE_LEVEL     HIGH
#define PIN_STATUS_LED        35       // On-board LED
#define PIN_PRG_BUTTON        0        // PRG/User button (also boot-mode select)

// --- Battery voltage sense (only meaningful if LiPo is actually connected —
//     see ReQ_loRa.md Section 5 "Power": this ADC reads the LiPo VBAT net,
//     NOT the general 3V3 rail. Guard reads with BATTERY_SENSE_ENABLED.) ---
#define PIN_BATTERY_ADC_EN     37       // Drive HIGH before sampling PIN_BATTERY_ADC
#define PIN_BATTERY_ADC        1        // ADC1_CH0
// VBAT = 100 / (100 + 390) * V(GPIO1)   (on-board divider, see datasheet 2.2.2)
#define BATTERY_DIVIDER_R1_KOHM   390.0f
#define BATTERY_DIVIDER_R2_KOHM   100.0f
#define BATTERY_ADC_SETTLE_MS     2      // settle time after enabling divider, before ADC read

// Set to false for deployments powered directly via the J3 3V3 pins
// (R1: "direct 3.3V" mode) where no LiPo is present and this reading
// would be meaningless.
#define BATTERY_SENSE_ENABLED     true

// =========================================================================
// 3. EXTERNAL SENSOR WIRING (Device 1 only, free header pins —
//    see architecture.md Section 3.2)
// =========================================================================

// --- BMP280 (temperature + pressure) — shares the OLED I2C bus ---
#define PIN_BMP280_SDA        PIN_OLED_SDA   // GPIO17, shared bus
#define PIN_BMP280_SCL        PIN_OLED_SCL   // GPIO18, shared bus
#define BMP280_I2C_ADDR_PRIMARY  0x76
#define BMP280_I2C_ADDR_SECONDARY 0x77       // fallback if SDO is tied high
#define I2C_CLOCK_HZ            100000        // 100 kHz standard mode; safe for shared bus w/ OLED

// --- DS18B20 (OneWire temperature) ---
#define PIN_DS18B20_DATA       4             // free RTC-capable GPIO, needs external 4.7k pull-up to 3V3
#define DS18B20_RESOLUTION_BITS   12          // 12-bit = 750 ms conversion time
#define ONEWIRE_PARASITE_POWER    false       // assume external power to DS18B20 (not parasitic)

// --- Reed switch (door sensor) ---
#define PIN_REED_SWITCH         3             // free RTC-capable GPIO -> ext1 deep-sleep wake source
#define REED_SWITCH_ACTIVE_LEVEL  LOW          // switch pulls to GND when closed; external pull-up assumed
#define REED_WAKE_ON_LEVEL       0             // ext1 wake level (0 = wake on LOW), match ACTIVE_LEVEL above

// =========================================================================
// 4. SPI BUS CONFIGURATION (shared by LoRa radio; Flash uses its own
//    separate SPI per datasheet 2.2.4 — not user-configurable here)
// =========================================================================
#define LORA_SPI_CLOCK_HZ        8000000      // 8 MHz — safe default for SX1262 over short on-board traces
#define LORA_SPI_BIT_ORDER       MSBFIRST
#define LORA_SPI_MODE            SPI_MODE0

// =========================================================================
// 5. LoRa RADIO PARAMETERS (SX1262, via RadioLib)
// =========================================================================
// NOTE (Open Point, see architecture.md Section 6): confirm regional
// frequency plan / duty-cycle limits for the actual deployment region
// before going live. Defaults below target EU868.
#define LORA_FREQUENCY_MHZ       868.1f       // EU868 default channel; hardware supports 863-928 MHz
#define LORA_BANDWIDTH_KHZ       125.0f
#define LORA_SPREADING_FACTOR    9            // SF7 (fast/short range) .. SF12 (slow/long range); SF9 = balanced
#define LORA_CODING_RATE         7            // 4/7
#define LORA_SYNC_WORD           0x12         // private network sync word (0x34 = public LoRaWAN, do not use)
#define LORA_PREAMBLE_LENGTH     8            // symbols
#define LORA_OUTPUT_POWER_DBM    14           // start conservative; board supports up to 28+-1 dBm high-power variant
#define LORA_CURRENT_LIMIT_MA    120          // SX1262 PA current limit
#define LORA_TCXO_VOLTAGE        1.6f         // Heltec V4 uses TCXO; 0 if using plain XTAL
#define LORA_USE_REGULATOR_DCDC  true         // SX1262 internal DC-DC vs LDO

// EU868 duty cycle: 1% on this sub-band by default -- respect this in
// application-layer timing if transmitting more frequently than the
// 15-min / event-driven pattern already implies.
#define LORA_DUTY_CYCLE_LIMIT_PCT  1.0f

// Optional short RX window after each TX, for gateway ACKs (architecture.md 4.3)
#define LORA_RX_ACK_WINDOW_MS     1500
#define LORA_ACK_TIMEOUT_MS       1500
#define LORA_MAX_RETRIES          2

// =========================================================================
// 6. TIMING / DUTY CYCLE (application layer)
// =========================================================================
#define MEASUREMENT_INTERVAL_MIN   15                              // R5, R6
#define MEASUREMENT_INTERVAL_US    ((uint64_t)MEASUREMENT_INTERVAL_MIN * 60ULL * 1000000ULL)

#define REED_DEBOUNCE_MS          50    // debounce after ext1 wake before reading door state
#define WAKE_TX_MERGE_WINDOW_MS    200   // merge timer + reed events if both occur in this window (architecture 4.3)

#define BOOT_SETTLE_MS            10    // brief settle time after cold boot / deep-sleep wake before sensor reads
#define LONG_PRESS_MS             3000  // PRG button hold time to enter future debug/config mode

// =========================================================================
// 7. LoRa MESSAGE PROTOCOL (lora_protocol) — see architecture.md 4.5
// =========================================================================
#define MSG_TYPE_PERIODIC_SENSOR   0x01
#define MSG_TYPE_DOOR_ALARM        0x02
#define MSG_TYPE_HEARTBEAT_BATTERY 0x03

// Field sizes (bytes) — must match gateway_app decode logic exactly
#define PROTO_FIELD_NODE_ID_LEN     1
#define PROTO_FIELD_MSG_TYPE_LEN    1
#define PROTO_FIELD_SEQ_LEN         1
#define PROTO_FIELD_BATTERY_MV_LEN  2
#define PROTO_FIELD_CRC16_LEN       2

#define CRC16_POLY                0x1021  // CRC-16/CCITT-FALSE
#define CRC16_INIT                0xFFFF

// =========================================================================
// 8. DEEP SLEEP / WAKE CONFIGURATION
// =========================================================================
// Wake sources: RTC timer (measurement interval) + ext1 GPIO (reed switch).
// ULP coprocessor intentionally NOT used — see architecture.md 4.3 rationale
// (every wake ends in an SPI-driven LoRa TX, which ULP cannot perform).
#define DEEP_SLEEP_WAKE_EXT1_PIN_MASK   (1ULL << PIN_REED_SWITCH)
#define DEEP_SLEEP_WAKE_EXT1_MODE       ESP_EXT1_WAKEUP_ALL_LOW   // wake when reed pin goes LOW

// RTC memory retained across deep sleep (see architecture.md 4.1) — declare
// state variables with RTC_DATA_ATTR in node_app.cpp, not here; this file
// only holds the timing constants above.

// =========================================================================
// 9. MISC / BUILD METADATA
// =========================================================================
#define FIRMWARE_VARIANT          "device1-battery-node"
#define CONFIG_VERSION            1     // bump when breaking pin/protocol changes are made
