#include <Arduino.h>
#include "Config.h"
#include "MoodLight.h"
#include "EmotionEngine.h"
#include "SerialConsole.h"
#include "ButtonInput.h"

// App objects
MoodLight moodLight(PIN_LED_R, PIN_LED_G, PIN_LED_B, FADE_DURATION_MS, FADE_STEP_INTERVAL, GLOBAL_BRIGHTNESS);
EmotionEngine engine(moodLight);
SerialConsole console(moodLight, engine);
PresetState presetState;

// Boot self-test
static void bootRgbSelfTest() {
  pinMode(PIN_LED_R, OUTPUT); pinMode(PIN_LED_G, OUTPUT); pinMode(PIN_LED_B, OUTPUT);
  digitalWrite(PIN_LED_R, HIGH); digitalWrite(PIN_LED_G, HIGH); digitalWrite(PIN_LED_B, HIGH);
  Serial.println(F("[SELFTEST] RGB (Common-Anode) R->G->B->W"));
  digitalWrite(PIN_LED_R, LOW); delay(200); digitalWrite(PIN_LED_R, HIGH); delay(70);
  digitalWrite(PIN_LED_G, LOW); delay(200); digitalWrite(PIN_LED_G, HIGH); delay(70);
  digitalWrite(PIN_LED_B, LOW); delay(200); digitalWrite(PIN_LED_B, HIGH); delay(70);
  digitalWrite(PIN_LED_R, LOW); digitalWrite(PIN_LED_G, LOW); digitalWrite(PIN_LED_B, LOW); delay(200);
  digitalWrite(PIN_LED_R, HIGH); digitalWrite(PIN_LED_G, HIGH); digitalWrite(PIN_LED_B, HIGH);
  Serial.println(F("[SELFTEST] Done"));
}

// Heartbeat LED
static void heartbeat(uint32_t now) {
  static uint32_t last = 0;
  if ((uint32_t)(now - last) >= 500) {
    last = now;
    digitalWrite(PIN_HEART, !digitalRead(PIN_HEART));
  }
}

void setup(){
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
}

void loop(){
  const uint32_t now = millis();
  heartbeat(now);
  console.handle(now);
  handleButton(now, moodLight, engine, presetState);
  moodLight.update(now);
}
