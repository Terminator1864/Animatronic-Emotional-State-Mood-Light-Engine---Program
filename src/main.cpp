#include <Arduino.h>
#include "Config.h"
#include "MoodLight.h"
#include "EmotionEngine.h"
#include "SerialConsole.h"
#include "ButtonInput.h"
#include "ModeManager.h"
#include "SensorInput.h"

// ===== App Objects =====
MoodLight      moodLight(PIN_LED_R, PIN_LED_G, PIN_LED_B, FADE_DURATION_MS, FADE_STEP_INTERVAL, GLOBAL_BRIGHTNESS);
EmotionEngine  engine(moodLight);
SerialConsole  console(moodLight, engine);
PresetState    presetState;

static ModeManager gMode;     // ACTIVE by default
static SensorInput gSensors;  // neutral stub today
static uint8_t s_lastEp = 255;

// ===== Boot Self-Test =====
static void bootRgbSelfTest() {
  pinMode(PIN_LED_R, OUTPUT); pinMode(PIN_LED_G, OUTPUT); pinMode(PIN_LED_B, OUTPUT);
  // Common-anode: LOW = on, HIGH = off
  Serial.println(F("[SELFTEST] RGB (Common-Anode) R->G->B->W"));
  digitalWrite(PIN_LED_R, LOW);  delay(200); digitalWrite(PIN_LED_R, HIGH); delay(60);
  digitalWrite(PIN_LED_G, LOW);  delay(200); digitalWrite(PIN_LED_G, HIGH); delay(60);
  digitalWrite(PIN_LED_B, LOW);  delay(200); digitalWrite(PIN_LED_B, HIGH); delay(60);
  // White
  digitalWrite(PIN_LED_R, LOW);  digitalWrite(PIN_LED_G, LOW);  digitalWrite(PIN_LED_B, LOW);  delay(200);
  digitalWrite(PIN_LED_R, HIGH); digitalWrite(PIN_LED_G, HIGH); digitalWrite(PIN_LED_B, HIGH);
  Serial.println(F("[SELFTEST] Done"));
}

// ===== Heartbeat LED (500 ms toggle) =====
static void heartbeat(uint32_t now) {
  static uint32_t last = 0;
  static bool on = false;
  if ((now - last) >= 500) {
    last = now;
    on = !on;
    digitalWrite(PIN_HEART, on ? HIGH : LOW);
  }
}

void setup() {
  pinMode(PIN_HEART, OUTPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  Serial.begin(115200);
  delay(60);
  Serial.println(F("[BOOT] Mood RGB Demo (Common-Anode)"));
  Serial.print  (F("[INFO] Pins R/G/B = ")); Serial.print(PIN_LED_R); Serial.print(F("/"));
  Serial.print  (PIN_LED_G); Serial.print(F("/")); Serial.println(PIN_LED_B);

  console.printHelp();
  bootRgbSelfTest();

  moodLight.begin();
  engine.begin(millis());
  gSensors.begin();
  
  console.attachSensorInput(&gSensors);
  Serial.print(F("[SENSE] AccelPresent="));
  Serial.println(gSensors.isPresent() ? F("YES") : F("NO"));

  // Serial MODE support + status
  console.attachModeManager(&gMode);
  gMode.printStatus(); // [MODE] ACTIVE

  // Quad-tap -> toggle mode (initialized inside ButtonInput.cpp without captures)
  ButtonInput_initForModeToggle(&gMode);
}

void loop() {
  const uint32_t now = millis();
  heartbeat(now);
  console.handle(now);
  handleButton(now, moodLight, engine, presetState);
  moodLight.update(now);

  const SensorSignals sigs = gSensors.sample(now);
  if (gMode.get() == RunMode::ACTIVE && sigs.valid) {
    uint8_t ep = map(sigs.arousalBias, 0, 255, 35, 200);
    engine.setPatternPenalty(ep);
    if (ep != s_lastEp) { s_lastEp = ep; Serial.print(F("[SENSE->ENGINE] PatternPenalty=")); Serial.println(ep); }

    // (optional) small brightness nudge
    uint8_t baseB = GLOBAL_BRIGHTNESS;
    uint8_t bump  = sigs.arousalBias / 15;
    uint16_t gb16 = (uint16_t)baseB + bump;
    moodLight.setGlobalBrightness((uint8_t)((gb16 > 255)? 255 : gb16));

    // FEED THE BIAS (this is required)
    engine.setExternalBias(sigs.arousalBias, sigs.valenceBias, true);
  } else {
    engine.setExternalBias(128, 128, false);
  }
}
