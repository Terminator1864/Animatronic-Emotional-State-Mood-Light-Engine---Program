#pragma once
#include <Arduino.h>
#include "Config.h"
#include "MoodLight.h"
#include "EmotionEngine.h"
#include "PresetSelector.h"

struct PresetState {
  bool     active = false;
  uint8_t  sel    = 1;
  uint32_t lastActivity = 0;
};

// === Quad-Tap Callback Type ===
typedef void (*QuadTapCallback)();   // e.g., [](){ gMode.toggle(); }

// === C-Style Module State For Multi-Tap ===
struct ButtonMultiTapState {
  uint8_t  tapCount = 0;
  uint32_t lastTapMs = 0;
  bool     suppressAggregation = false; // set true when long / very-long recognized
  QuadTapCallback quadTapCb = nullptr;
};

// === Public API (call from main.cpp / setup) ===
void ButtonInput_attachQuadTap(ButtonMultiTapState& st, QuadTapCallback cb);

// Optional convenience: initialize quad-tap to toggle ModeManager without exposing state.
class ModeManager; // fwd-declare
void ButtonInput_initForModeToggle(ModeManager* mode);

// === Hooks You Call From Your Existing FSM Paths ===
void ButtonInput_onShortRelease(uint32_t nowMs, ButtonMultiTapState& st);
void ButtonInput_onLongRecognized(ButtonMultiTapState& st);
void ButtonInput_onVeryLongRecognized(ButtonMultiTapState& st);
void ButtonInput_onIdle(uint32_t nowMs, ButtonMultiTapState& st);

// === Your Existing Handler (unchanged signature)
void handleButton(uint32_t now, MoodLight& ml, EmotionEngine& engine, PresetState& ps);
