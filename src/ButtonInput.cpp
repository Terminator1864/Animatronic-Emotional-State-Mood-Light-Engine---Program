#include "ButtonInput.h"
#include "ModeManager.h"

// Private module state
static ButtonMultiTapState sMultiTap;
static ModeManager* sModePtr = nullptr;

// Non-capturing thunk so we can use a plain function pointer
static void _quadTapThunk() {
  if (sModePtr) sModePtr->toggle();
}

// === Internal Helpers ===
static void resetAggregation_(ButtonMultiTapState& st) {
  st.tapCount = 0;
  st.lastTapMs = 0;
  st.suppressAggregation = false;
}

void ButtonInput_attachQuadTap(ButtonMultiTapState& st, QuadTapCallback cb) {
  st.quadTapCb = cb;
  resetAggregation_(st);
}

// Convenience: hide sMultiTap and wire directly to ModeManager
void ButtonInput_initForModeToggle(ModeManager* mode) {
  sModePtr = mode;
  ButtonInput_attachQuadTap(sMultiTap, &_quadTapThunk); // non-capturing, OK for function ptr
}

void ButtonInput_onShortRelease(uint32_t nowMs, ButtonMultiTapState& st) {
  if (st.suppressAggregation) return;
  if ((nowMs - st.lastTapMs) > MULTITAP_WINDOW_MS) st.tapCount = 0;
  st.lastTapMs = nowMs;
  if (st.tapCount < 255) st.tapCount++;
  if (st.tapCount >= MULTITAP_TOGGLE_COUNT) {
    st.tapCount = 0;
    if (st.quadTapCb) {
      Serial.println(F("[BTN] Quad-Tap Detected -> Toggle Mode"));
      st.quadTapCb();
    }
  }
}

void ButtonInput_onLongRecognized(ButtonMultiTapState& st) {
  st.suppressAggregation = true;
  resetAggregation_(st);
}

void ButtonInput_onVeryLongRecognized(ButtonMultiTapState& st) {
  st.suppressAggregation = true;
  resetAggregation_(st);
}

void ButtonInput_onIdle(uint32_t nowMs, ButtonMultiTapState& st) {
  if (st.lastTapMs == 0) return;
  if ((nowMs - st.lastTapMs) > MULTITAP_WINDOW_MS) st.tapCount = 0;
}

void handleButton(uint32_t now, MoodLight& ml, EmotionEngine& engine, PresetState& ps){
  static uint8_t  lastState   = HIGH;
  static uint32_t lastChange  = 0;
  static uint32_t pressStart  = 0;

  // Auto-timeout preset mode
  if (ps.active && (uint32_t)(now - ps.lastActivity) > PRESET_IDLE_TIMEOUT_MS){
    ps.active = false;
    Serial.println(F("[PRESET] Timeout -> Exit (no change)"));
  }

  uint8_t s = digitalRead(PIN_BUTTON);
  if (s == lastState) return;
  if ((uint32_t)(now - lastChange) < 30) return; // debounce
  lastChange = now;
  lastState  = s;

  if (s == LOW){ // pressed
    pressStart = now;
    return;
  }

  // released
  uint32_t held = now - pressStart;

  if (ps.active){
    if (held >= LONG_HOLD_MS){
      const char* moodName = presetNameByIndex(ps.sel);
      const char* dispName = presetDisplayName(ps.sel);
      bool ok = ml.setMoodByName(moodName, now);
      if (ok) Serial.print(F("[PRESET] Apply -> "));
      else    Serial.print(F("[PRESET] ERROR applying -> "));
      Serial.println(dispName);
      ps.active = false;
      return;
    }else{
      ButtonInput_onVeryLongRecognized(sMultiTap);
      ps.sel++; if (ps.sel>6) ps.sel=1;
      ps.lastActivity = now;
      Serial.print(F("[PRESET] Select ")); Serial.print(ps.sel);
      Serial.print(F(" -> ")); Serial.println(presetDisplayName(ps.sel));
      return;
    }
  }

  // not in preset mode
  if (held >= VERY_LONG_HOLD_MS && ml.isFrozen()){
    ps.active = true; ps.sel = 1; ps.lastActivity = now;
    Serial.println(F("[PRESET] Mode=ON | Hold to Apply, Short to Cycle 1..6"));
    Serial.print  (F("[PRESET] Select 1 -> ")); Serial.println(presetDisplayName(1));
    return;
  }

  if (held >= LONG_HOLD_MS){
    ButtonInput_onLongRecognized(sMultiTap);
    ml.freezeHold(!ml.isFrozen());
    Serial.print(F("[BTN] Freeze Toggle -> "));
    Serial.println(ml.isFrozen()?F("ON"):F("OFF"));
    return;
  }

  // short press
  if (ml.isFrozen()){
    ButtonInput_onShortRelease(now, sMultiTap);
    Serial.println(F("[BTN] Ignored Next (Frozen)"));
    return;
  }
  // delegate choice to engine style next
  engine.operatorNext(now);
  Serial.println(F("[BTN] Next Mood"));
}

