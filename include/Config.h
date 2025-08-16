#pragma once
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

// Button thresholds
static constexpr uint16_t LONG_HOLD_MS = 700;
static constexpr uint16_t VERY_LONG_HOLD_MS = 1400;

// Preset select
static constexpr uint32_t PRESET_IDLE_TIMEOUT_MS = 8000;

// Emotion engine
static constexpr uint8_t DEFAULT_PATTERN_PENALTY = 120;
