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
  for (uint8_t k=0;k<HIST_N;k++){
    if (history[k] == toIdx) {
      uint8_t factor = (uint8_t)((HIST_N - k) * 40); // stronger push to diversify
      pen += factor;
    }
  }
  uint16_t maxPen = 240;
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
  extArousal  = arousalBias;
  extValence  = valenceBias;
  extBiasValid = valid;
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

// Turn external bias (0..255) into signed [-100..+100], dot with mood, scale → 0..200
uint16_t EmotionEngine::biasWeight(uint8_t toIdx) const {
  if (!extBiasValid) return 0;

  // Map 0..255 -> -100..+100 (128~neutral)
  int16_t bA = ((int16_t)extArousal * 200 / 255) - 100;
  int16_t bV = ((int16_t)extValence * 200 / 255) - 100;

  EmotionVec mv = moodVec(toIdx);

  // Dot product in [-20000..+20000]
  int32_t dot = (int32_t)bA * (int32_t)mv.arousal + (int32_t)bV * (int32_t)mv.valence;

  // Scale to approx 0..200; ignore negatives (don’t punish, just don’t boost)
  // 20000 / 100 = 200
  int16_t boost = (int16_t)(dot / 100);
  if (boost < 0) boost = 0;
  if (boost > 200) boost = 200;
  return (uint16_t)boost;
}
