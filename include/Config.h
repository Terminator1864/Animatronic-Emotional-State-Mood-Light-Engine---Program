#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// === Pins ===
static constexpr uint8_t PIN_LED_R = 11;  // PWM
static constexpr uint8_t PIN_LED_G = 10;  // PWM
static constexpr uint8_t PIN_LED_B = 9;   // PWM
static constexpr uint8_t PIN_HEART = LED_BUILTIN;
static constexpr uint8_t PIN_BUTTON = 2;  // INPUT_PULLUP

// === Timing ===
static constexpr uint16_t FADE_DURATION_MS   = 1100;
static constexpr uint16_t FADE_STEP_INTERVAL = 20;
static constexpr uint8_t  GLOBAL_BRIGHTNESS  = 200;

// === Mode / Multi-Tap Timing ===
static const uint16_t MULTITAP_WINDOW_MS    = 600;  
static const uint8_t  MULTITAP_TOGGLE_COUNT = 4;    

// Button thresholds
static constexpr uint16_t LONG_HOLD_MS = 700;
static constexpr uint16_t VERY_LONG_HOLD_MS = 1400;

// Preset select
static constexpr uint32_t PRESET_IDLE_TIMEOUT_MS = 8000;

// Emotion engine
static constexpr uint8_t DEFAULT_PATTERN_PENALTY = 120;

// === LSM303DLHC (Adafruit) Over I2C ===
// UNO I2C pins: SDA=A4, SCL=A5. Keep wires short; add 0.1µF + 10µF near sensor.
#define LSM303_ACCEL_ADDR   0x19  // most Adafruit DLHC boards (SA0=HIGH). Try 0x18 if needed.

// I2C & Sampling
static const uint16_t LSM303_I2C_CLOCK_KHZ      = 100;  // 100 kHz = safer cabling; bump to 400 if rock solid
static const uint16_t ACCEL_SAMPLE_INTERVAL_MS  = 16;   // ~62.5 Hz
static const uint8_t  ACCEL_EWMA_ALPHA          = 24;   // 0..255 (higher = snappier)
static const uint16_t ACCEL_GATE_MIN_DELTA      = 6;   // noise gate on motion envelope (raw L1 delta)
static const uint8_t  ACCEL_SCALE_SHIFT         = 4;    // maps motion → 0..255 (delta >> shift)

#endif // CONFIG_H