#include "EmotionEngine.h"
#include "MoodLight.h" // for PatternType names if needed

void EmotionEngine::begin(uint32_t nowMs){
  (void)nowMs;
  rng ^= (uint16_t)micros();
  for (uint8_t i=0;i<HIST_N;i++) history[i]=255;
}

void EmotionEngine::operatorNext(uint32_t nowMs){
  (void)nowMs;
  const uint8_t cur = currentIdx();
  const uint8_t count = target.moodCount();
  uint16_t w[32]; // enough

  for (uint8_t i=0;i<count;i++){
    uint16_t total = 0;
    total += baseWeight(i);
    total += adjacencyWeight(cur,i);
    total += recencyWeight(i);
    total += patternRecencyWeight(cur,i);
    total += biasWeight(i);
    w[i] = total;
  }
  uint8_t next = pickWeighted(w, count);

  // No-immediate-repeat vs current and last
  if (next == cur) next = (uint8_t)((cur + 1) % count);
  uint8_t last = history[(historyIdx + HIST_N - 1) % HIST_N];
  if (last != 255 && next == last) next = (uint8_t)((next + 1) % count);

  if (target.setMoodByIndex(next, millis())) pushHistory(next);
}

void EmotionEngine::pushHistory(uint8_t idx){
  history[historyIdx++] = idx;
  if (historyIdx >= HIST_N) historyIdx = 0;
}

uint16_t EmotionEngine::urand(){
  // x^16 + x^14 + x^13 + x^11 taps
  uint16_t lfsr = rng;
  uint16_t bit = ((lfsr>>0) ^ (lfsr>>2) ^ (lfsr>>3) ^ (lfsr>>5)) & 1u;
  lfsr = (uint16_t)((lfsr>>1) | (bit<<15));
  rng = lfsr;
  return lfsr;
}

uint8_t EmotionEngine::pickWeighted(uint16_t* w, uint8_t count){
  uint32_t sum = 0;
  for (uint8_t i=0;i<count;i++) sum += w[i];
  if (sum == 0) return (uint8_t)(urand() % count);
  uint32_t r = urand();
  uint32_t pickScaled = (r * sum) >> 16;
  uint32_t acc = 0;
  for (uint8_t i=0;i<count;i++){
    acc += w[i];
    if (pickScaled < acc) return i;
  }
  return (uint8_t)(count-1);
}

uint16_t EmotionEngine::baseWeight(uint8_t toIdx) const {
  (void)toIdx; return 200;
}

uint16_t EmotionEngine::adjacencyWeight(uint8_t fromIdx, uint8_t toIdx) const {
  if (fromIdx == toIdx) return 40;
  EmotionVec a = moodVec(fromIdx);
  EmotionVec b = moodVec(toIdx);
  int16_t dv = (int16_t)a.valence - (int16_t)b.valence;
  int16_t da = (int16_t)a.arousal - (int16_t)b.arousal;
  uint16_t dist = (uint16_t)(abs(dv) + abs(da));
  int16_t w = 255 - (int16_t)(dist * 2);
  if (w < 0) w = 0;
  return (uint16_t)w;
}

uint16_t EmotionEngine::recencyWeight(uint8_t toIdx) const {
  uint16_t pen = 0;
  // age = 0 is most recent (last written), up to HIST_N-1 is oldest
  for (uint8_t age = 0; age < HIST_N; ++age){
    uint8_t k = (uint8_t)((historyIdx + HIST_N - 1 - age) % HIST_N);
    if (history[k] == toIdx) {
      uint8_t factor = (uint8_t)((HIST_N - age) * 40);
      pen += factor;
    }
  }
  const uint16_t maxPen = 240;
  uint16_t p = (pen > maxPen) ? maxPen : pen;
  return (uint16_t)(maxPen - p);
}

uint16_t EmotionEngine::patternRecencyWeight(uint8_t fromIdx, uint8_t toIdx) const {
  PatternType pf = target.patternOfIndex(fromIdx);
  PatternType pt = target.patternOfIndex(toIdx);
  if (pf != pt) return 200;
  // same pattern -> penalty
  uint16_t pen = (uint16_t)patternPenalty;
  return (uint16_t)(200 - pen); // lower is worse; combined with others
}

void EmotionEngine::setExternalBias(uint8_t arousalBias, uint8_t valenceBias, bool valid){
  extBiasValid = valid;
  if (!valid) return;

  const uint8_t K = 2; // larger K = more smoothing (K=2 is gentle)
  extArousal += (int16_t(arousalBias) - extArousal) >> K;
  extValence += (int16_t(valenceBias) - extValence) >> K;
}

// Map each Mood to a signed (valence, arousal) in [-100..+100]
EmotionVec EmotionEngine::moodVec(uint8_t i) const {
  using M = Mood;
  switch ((M)i){
    case M::Serenity:      return { +70,  -60 };
    case M::Joy:           return { +90,  +40 };
    case M::Excitement:    return { +85,  +90 };
    case M::Love:          return { +95,  +45 };
    case M::Pride:         return { +65,  +35 };
    case M::Determination: return { +40,  +55 };
    case M::Playful:       return { +75,  +65 };
    case M::Curiosity:     return { +40,  +30 };
    case M::Confusion:     return { -20,  +25 };
    case M::Surprise:      return { +10,  +85 }; // near-neutral valence, high arousal
    case M::Sadness:       return { -80,  -50 };
    case M::Melancholy:    return { -60,  -65 };
    case M::Anger:         return { -70,  +75 }; // negative valence, high arousal
    case M::Panic:         return { -90,  +95 };
    case M::Fear:          return { -85,  +80 };
    case M::Sleepy:        return {  +5,  -90 };
    default:               return {   0,    0 };
  }
}

// Turn external bias (0..255) into weighted boost that works for calm and active.
// - extArousal: 0 = very calm, 255 = very active
// - extValence: 0..255 (128 ≈ neutral). If you don't feed valence yet, it will be ~128.
uint16_t EmotionEngine::biasWeight(uint8_t toIdx) const {
  if (!extBiasValid) return 0;

  EmotionVec mv = moodVec(toIdx);     // mv.valence, mv.arousal in [-100..+100]

  // ---- AROUSAL TERM (two-sided but always positive gain) ----
  // Boost high-arousal moods proportionally to extArousal,
  // and boost low-arousal moods when extArousal is low.
  // Both parts are in 0..100, sum to 0..200.
  uint16_t posA = (mv.arousal > 0) ? (uint16_t)mv.arousal : 0;   // 0..100
  uint16_t negA = (mv.arousal < 0) ? (uint16_t)(-mv.arousal) : 0;// 0..100

  // Scale extArousal 0..255 → 0..100 with integer math
  uint16_t aHi  = (uint16_t)((posA * (uint16_t)extArousal) / 255);          // 0..100
  uint16_t aLow = (uint16_t)((negA * (uint16_t)(255 - extArousal)) / 255);  // 0..100
  uint16_t arousalBoost = aHi + aLow;                                       // 0..200

  // ---- VALENCE TERM (signed; still clamps to positive only) ----
  // Map 0..255 → -100..+100 (128 ≈ 0)
  int16_t bV = ((int16_t)extValence * 200 / 255) - 100;      // -100..+100
  // dot valence; keep positive component only (don’t punish)
  int16_t vDot = (int16_t)mv.valence * bV;                   // [-10000..+10000]
  int16_t valenceBoost = vDot / 100;                         // [-100..+100]
  if (valenceBoost < 0) valenceBoost = 0;                    // 0..100

  // ---- Combine and clamp ----
  uint16_t boost = arousalBoost + (uint16_t)valenceBoost;    // 0..300
  boost = (boost * 3) / 2;
  if (boost > 200) boost = 200;                              // cap to match other weights

  return boost;
}

