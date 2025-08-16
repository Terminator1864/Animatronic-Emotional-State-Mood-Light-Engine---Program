# RGB_ProgV3_Modular_Deluxe

Modular Arduino (UNO) project: RGB mood light with Emotion Engine, freeze, 6 basic emotion presets, pattern-recency penalty, and serial console.

**Button Mapping**
- Short press: Next mood (ignored while frozen)
- Long (≥700ms): Freeze / Unfreeze
- Very long (≥1400ms) while frozen: Enter Preset Select. Short=cycle 1..6; Long=apply+exit; times out in 8s.

**Serial Commands**
`N` (Next), `F` (Freeze), `B:<0-255>` (brightness), `M:<name>`, `M#:<index>`, `EP:<0-255>` (pattern penalty), `?` (help)

Open serial monitor @115200.
