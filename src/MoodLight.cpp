#include "MoodLight.h"
#include "EmotionEngine.h"
extern EmotionEngine engine;    

// ===== Palette (16 moods) =====
const MoodDef MoodLight::MOODS[(int)Mood::Count] = {
  { Mood::Serenity , "Serenity" , "Soft Aqua" , "-"        , {  0,170,255}, {  0,  0,  0}, PatternType::Breathe ,  50, 2600, 1400 },
  { Mood::Joy      , "Joy"      , "Warm Gold" , "-"        , {255,195, 60}, {  0,  0,  0}, PatternType::Breathe ,  90, 1800, 1300 },
  { Mood::Excitement,"Excitement","Hot Pink" , "-"         , {255,  0,200}, {  0,  0,  0}, PatternType::Pulse   , 160,  480, 1100 },
  { Mood::Love     , "Love"     , "Rose"      , "-"        , {255, 60,120}, {  0,  0,  0}, PatternType::Heartbeat,110,  900, 1300 },
  { Mood::Pride    , "Pride"    , "Royal Purple","-"       , {160,  0,200}, {  0,  0,  0}, PatternType::Breathe ,  60, 2200, 1300 },
  { Mood::Determination,"Determination","Amber","-"        , {230,120,  0}, {  0,  0,  0}, PatternType::Pulse   ,  90,  900, 1400 },
  { Mood::Playful  , "Playful"  , "Cyan"      , "Magenta"  , {  0,255,255}, {255,  0,255}, PatternType::BlinkAlt,   0,  600, 1200 },
  { Mood::Curiosity,"Curiosity" , "Teal"      , "Lime"     , {  0,200,160}, {140,255,  0}, PatternType::BlinkAlt,   0,  800, 1300 },
  { Mood::Confusion,"Confusion" , "Blue"      , "Yellow"   , { 40,120,255}, {255,200,  0}, PatternType::BlinkAlt,   0,  700, 1200 },
  { Mood::Surprise , "Surprise" , "White"     , "-"        , {255,255,255}, {  0,  0,  0}, PatternType::Pulse   , 200,  320,  900 },
  { Mood::Sadness  , "Sadness"  , "Deep Blue" , "-"        , {  0,  0,180}, {  0,  0,  0}, PatternType::Breathe ,  40, 3200, 1600 },
  { Mood::Melancholy,"Melancholy","Muted Blue","-"         , { 20, 40,120}, {  0,  0,  0}, PatternType::Breathe ,  25, 3800, 1600 },
  { Mood::Anger    , "Anger"    , "Red"       , "-"        , {255,  0,  0}, {  0,  0,  0}, PatternType::Heartbeat,150,  850, 1100 },
  { Mood::Panic    , "Panic"    , "Red-White" , "-"        , {255,120,120}, {  0,  0,  0}, PatternType::Pulse   , 220,  420, 1000 },
  { Mood::Fear     , "Fear"     , "Dim Violet", "-"        , {120,  0,180}, {  0,  0,  0}, PatternType::Flicker ,  40,  120, 1300 },
  { Mood::Sleepy   , "Sleepy"   , "Warm Amber", "-"        , {180, 70,  0}, {  0,  0,  0}, PatternType::Breathe ,  35, 4200, 1600 }
};

MoodLight::MoodLight(uint8_t pinRedPwm, uint8_t pinGreenPwm, uint8_t pinBluePwm,
                     uint16_t fadeDurationMs, uint16_t fadeStepMs, uint8_t globalBrightness0to255)
: pinR(pinRedPwm), pinG(pinGreenPwm), pinB(pinBluePwm),
  fadeTotalMs(fadeDurationMs), fadeStepIntervalMs(fadeStepMs),
  globalBrightness(globalBrightness0to255),
  isInit(false), moodIndex(0), lastStepMs(0), holdStartMs(0),
  isHolding(false), printedStatusThisHold(false), freezeMode(false),
  startColor{0,0,0}, targetColor{0,0,0}, stepsPlanned(0), stepNumber(0), lfsr(0xACE1u)
{
  if (!fadeStepIntervalMs) fadeStepIntervalMs = 20;
  if (fadeTotalMs < fadeStepIntervalMs) fadeTotalMs = fadeStepIntervalMs;
}

void MoodLight::begin() {
  pinMode(pinR, OUTPUT); pinMode(pinG, OUTPUT); pinMode(pinB, OUTPUT);
  digitalWrite(pinR, HIGH); digitalWrite(pinG, HIGH); digitalWrite(pinB, HIGH); // CA off
  lfsr ^= (uint32_t)micros();
  setTargetFromMood(moodIndex);
  startColor = {0,0,0};
  startFade(millis());
  isInit = true;
}

void MoodLight::update(uint32_t nowMs) {
  if (!isInit) return;

 if (isHolding) {
    updateHoldPattern(nowMs);

    if (!printedStatusThisHold) {
      printStatusLine();
      printedStatusThisHold = true;
    }

    if (freezeMode) return; // stay in this mood until unfrozen

    uint16_t holdMs = MOODS[moodIndex].holdMs;
    holdMs = (uint16_t)((uint32_t)holdMs * holdScalePct_ / 100);

    if ((uint32_t)(nowMs - holdStartMs) < holdMs) return;

    // Time to move on - let the engine pick next based on bias/history
    engine.operatorNext(nowMs);
    return;
  }

  if ((uint32_t)(nowMs - lastStepMs) < fadeStepIntervalMs) return;

  lastStepMs = nowMs;
  if (stepNumber >= stepsPlanned) {
    writeCommonAnodePwm(targetColor);
    isHolding = true;
    printedStatusThisHold = false;
    holdStartMs = nowMs;
    return;
  }

  stepFadeOnce();

}

// === Control / Telemetry
void MoodLight::setGlobalBrightness(uint8_t b) { globalBrightness = b; }
const char* MoodLight::currentMoodName() const { return MOODS[moodIndex].nameCStr; }
const char* MoodLight::currentPatternName() const { return patternName(MOODS[moodIndex].pattern); }
uint8_t MoodLight::currentAmp() const { return MOODS[moodIndex].amp0to255; }
uint16_t MoodLight::currentPeriodMs() const { return MOODS[moodIndex].periodMs; }

PatternType MoodLight::patternOfIndex(uint8_t idx) const {
  if (idx >= (uint8_t)Mood::Count) idx = 0;
  return MOODS[idx].pattern;
}

Rgb8 MoodLight::currentBaseColorScaled() const {
  Rgb8 c = MOODS[moodIndex].baseColor;
  c.r = scaleAndClamp(c.r, globalBrightness);
  c.g = scaleAndClamp(c.g, globalBrightness);
  c.b = scaleAndClamp(c.b, globalBrightness);
  return c;
}
Rgb8 MoodLight::currentAltColorScaled() const {
  Rgb8 c = MOODS[moodIndex].altColor;
  c.r = scaleAndClamp(c.r, globalBrightness);
  c.g = scaleAndClamp(c.g, globalBrightness);
  c.b = scaleAndClamp(c.b, globalBrightness);
  return c;
}

bool MoodLight::setMoodByIndex(uint8_t idx, uint32_t nowMs) {
  if (idx >= (uint8_t)Mood::Count) return false;
  Rgb8 prev = targetColor;
  moodIndex = idx;
  setTargetFromMood(moodIndex);
  startColor = prev;
  startFade(nowMs);
  return true;
}

bool MoodLight::equalsIgnoreCase(const char* a, const char* b) {
  if (!a || !b) return false;
  while (*a && *b) {
    char ca = *a++, cb = *b++;
    if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
    if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
    if (ca != cb) return false;
  }
  return *a == 0 && *b == 0;
}

bool MoodLight::setMoodByName(const char* name, uint32_t nowMs){
  if (!name) return false;
  for (uint8_t i=0;i<(uint8_t)Mood::Count;i++){
    if (equalsIgnoreCase(name, MOODS[i].nameCStr)){
      return setMoodByIndex(i, nowMs);
    }
  }
  return false;
}

void MoodLight::jumpToNext(uint32_t nowMs) { advanceToNextMood(nowMs); }
void MoodLight::freezeHold(bool enable) { freezeMode = enable; }

// === Flow helpers
void MoodLight::advanceToNextMood(uint32_t nowMs) {
  if (++moodIndex >= (uint8_t)Mood::Count) moodIndex = 0;
  setTargetFromMood(moodIndex);
  startColor = targetColor;
  startFade(nowMs);
}

void MoodLight::setTargetFromMood(uint8_t idx) {
  if (idx >= (uint8_t)Mood::Count) idx = 0;
  Rgb8 c = MOODS[idx].baseColor;
  c.r = scaleAndClamp(c.r, globalBrightness);
  c.g = scaleAndClamp(c.g, globalBrightness);
  c.b = scaleAndClamp(c.b, globalBrightness);
  targetColor = c;
}

void MoodLight::startFade(uint32_t nowMs) {
  isHolding = false; stepNumber = 0;
  stepsPlanned = (uint16_t)(fadeTotalMs / fadeStepIntervalMs);
  if (!stepsPlanned) stepsPlanned = 1;
  lastStepMs = nowMs;
}

void MoodLight::stepFadeOnce() {
  auto lerp8 = [](uint8_t a,uint8_t b,uint16_t n,uint16_t d)->uint8_t{
    if (!d) return b;
    int16_t diff=(int16_t)b-(int16_t)a;
    int32_t v=(int32_t)a+((int32_t)diff*(int32_t)n)/(int32_t)d;
    if (v<0) { v=0; }
    if (v>255) { v=255; }
    return (uint8_t)v; };
  Rgb8 out;
  out.r = lerp8(startColor.r, targetColor.r, stepNumber, stepsPlanned);
  out.g = lerp8(startColor.g, targetColor.g, stepNumber, stepsPlanned);
  out.b = lerp8(startColor.b, targetColor.b, stepNumber, stepsPlanned);
  writeCommonAnodePwm(out);
  stepNumber++;
}

void MoodLight::updateHoldPattern(uint32_t nowMs) {
  const MoodDef& md = MOODS[moodIndex];
  Rgb8 base = targetColor, out = base;

  switch (md.pattern) {
    case PatternType::Static: break;

    case PatternType::Breathe: {
      uint8_t w = triangleWave(nowMs - holdStartMs, md.periodMs);
      uint8_t m = (uint8_t)(((uint16_t)w * md.amp0to255)>>8);
      auto up=[](uint8_t v,uint8_t add)->uint8_t{ uint16_t s=v+add; return s>255?255:(uint8_t)s; };
      out.r=(uint8_t)(((uint16_t)base.r*(255u-m)+(uint16_t)up(base.r,md.amp0to255)*m)/255u);
      out.g=(uint8_t)(((uint16_t)base.g*(255u-m)+(uint16_t)up(base.g,md.amp0to255)*m)/255u);
      out.b=(uint8_t)(((uint16_t)base.b*(255u-m)+(uint16_t)up(base.b,md.amp0to255)*m)/255u);
      break; }

    case PatternType::Pulse: {
      uint8_t w = pulseWave(nowMs - holdStartMs, md.periodMs, 60);
      uint8_t m = (uint8_t)(((uint16_t)w * md.amp0to255)>>8);
      auto up=[](uint8_t v,uint8_t add)->uint8_t{ uint16_t s=v+add; return s>255?255:(uint8_t)s; };
      out.r=(uint8_t)(((uint16_t)base.r*(255u-m)+(uint16_t)up(base.r,md.amp0to255)*m)/255u);
      out.g=(uint8_t)(((uint16_t)base.g*(255u-m)+(uint16_t)up(base.g,md.amp0to255)*m)/255u);
      out.b=(uint8_t)(((uint16_t)base.b*(255u-m)+(uint16_t)up(base.b,md.amp0to255)*m)/255u);
      break; }

    case PatternType::Heartbeat: {
      uint8_t w = heartbeatWave(nowMs - holdStartMs, md.periodMs);
      uint8_t m = (uint8_t)(((uint16_t)w * md.amp0to255)>>8);
      auto up=[](uint8_t v,uint8_t add)->uint8_t{ uint16_t s=v+add; return s>255?255:(uint8_t)s; };
      out.r=(uint8_t)(((uint16_t)base.r*(255u-m)+(uint16_t)up(base.r,md.amp0to255)*m)/255u);
      out.g=(uint8_t)(((uint16_t)base.g*(255u-m)+(uint16_t)up(base.g,md.amp0to255)*m)/255u);
      out.b=(uint8_t)(((uint16_t)base.b*(255u-m)+(uint16_t)up(base.b,md.amp0to255)*m)/255u);
      break; }

    case PatternType::Flicker: {
      int16_t j = (int16_t)flickerJitter() - 128;
      int16_t d = ((int16_t)md.amp0to255 * j) / 128;
      auto addClamp=[](int16_t v,int16_t dd)->uint8_t{ int32_t s=(int32_t)v+dd; if(s<0)s=0; if(s>255)s=255; return (uint8_t)s; };
      out.r=addClamp(base.r,d); out.g=addClamp(base.g,d); out.b=addClamp(base.b,d);
      break; }

    case PatternType::BlinkAlt: {
      const uint32_t t=(nowMs - holdStartMs) % (uint32_t)md.periodMs;
      const bool useAlt = (t < (md.periodMs/2));
      out = useAlt ? currentAltColorScaled() : currentBaseColorScaled();
      break; }
  }
  writeCommonAnodePwm(out);
}

void MoodLight::writeCommonAnodePwm(const Rgb8& c) { 
  analogWrite(pinR,255-c.r); analogWrite(pinG,255-c.g); analogWrite(pinB,255-c.b); 
}

uint8_t MoodLight::scaleAndClamp(uint8_t v,uint8_t s) { 
  uint16_t r=(uint16_t)v*(uint16_t)s/255u; return (r>255u)?255u:(uint8_t)r; 
}

uint8_t MoodLight::triangleWave(uint32_t t, uint16_t p){
  if (p == 0) return 255;
  uint32_t m = t % p;
  uint32_t h = p / 2u;
  if (m < h) {
    return (uint8_t)((m * 255u) / (h ? h : 1u));
  } else {
    uint32_t d = m - h;
    return (uint8_t)(255u - (d * 255u) / (h ? h : 1u));
  }
}

uint8_t MoodLight::pulseWave(uint32_t t, uint16_t p, uint8_t duty){
  if (p == 0) return 255;
  uint32_t m   = t % p;
  uint32_t thr = ((uint32_t)p * duty) >> 8;   // duty in 0..255 (e.g., 60 ≈ 23%)
  return (m < thr) ? 255 : 0;
}

uint8_t MoodLight::heartbeatWave(uint32_t t, uint16_t p){
  if (p == 0) return 255;
  uint32_t m = t % p;

  uint32_t p1_on  = (p *  0) / 100;
  uint32_t p1_off = (p * 10) / 100;
  uint32_t p2_on  = (p * 14) / 100;
  uint32_t p2_off = (p * 24) / 100;

  if ((m >= p1_on && m < p1_off) || (m >= p2_on && m < p2_off)) return 255;

  uint32_t tail_len = (p * 8) / 100;
  if (m >= p2_off && m < (p2_off + tail_len)) {
    uint32_t d = m - p2_off;
    return (uint8_t)(200u - (d * 200u) / (tail_len ? tail_len : 1u));
  }
  return 0;
}

uint8_t MoodLight::flickerJitter() {
  uint16_t x = (uint16_t)(lfsr & 0xFFFFu);
  uint16_t bit = ((x >> 0) ^ (x >> 2) ^ (x >> 3) ^ (x >> 5)) & 1u;
  x = (uint16_t)((x >> 1) | (bit << 15));
  lfsr = (lfsr & 0xFFFF0000u) | x;
  return (uint8_t)(x >> 8);
}

const char* MoodLight::patternName(PatternType p) {
  switch (p) {
    case PatternType::Static: return "Static";
    case PatternType::Breathe: return "Breathe";
    case PatternType::Pulse: return "Pulse";
    case PatternType::Heartbeat: return "Heartbeat";
    case PatternType::Flicker: return "Flicker";
    case PatternType::BlinkAlt: return "BlinkAlt";
    default: return "Unknown";
  }
}

void MoodLight::printStatusLine(){
  const MoodDef& md = MOODS[moodIndex];
  Rgb8 base = currentBaseColorScaled();
  Serial.print(F("[MOOD] Emotion=")); Serial.print(md.nameCStr);
  Serial.print(F(" | Pattern=")); Serial.print(patternName(md.pattern));
  Serial.print(F(" | BaseColor=")); Serial.print(md.baseNameCStr);
  Serial.print(F(" rgb(")); Serial.print(base.r); Serial.print(F(",")); Serial.print(base.g); Serial.print(F(",")); Serial.print(base.b); Serial.print(F(")"));
  if (md.pattern == PatternType::BlinkAlt){
    Rgb8 alt=currentAltColorScaled();
    Serial.print(F(" | AltColor=")); Serial.print(md.altNameCStr);
    Serial.print(F(" rgb(")); Serial.print(alt.r); Serial.print(F(",")); Serial.print(alt.g); Serial.print(F(",")); Serial.print(alt.b); Serial.print(F(")"));
  }
  Serial.print(F(" | Amp=")); Serial.print(md.amp0to255);
  Serial.print(F(" | PeriodMs=")); Serial.print(md.periodMs);
  Serial.print(F(" | HoldMs=")); Serial.print(md.holdMs);
  Serial.print(F(" | GlobalBrightness=")); Serial.print(globalBrightness);
  Serial.print(F(" | Freeze=")); Serial.print(freezeMode ? F("ON") : F("OFF"));
  Serial.print(F(" | Pins R/G/B=")); Serial.print(pinR); Serial.print(F("/")); Serial.print(pinG); Serial.print(F("/")); Serial.print(pinB);
  Serial.println();
}

