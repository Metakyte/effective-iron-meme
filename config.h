#pragma once

// ================= Pins =================
#define MIC_SCK_PIN     14   // INMP441 SCK
#define MIC_WS_PIN      15   // INMP441 WS
#define MIC_SD_PIN      32   // INMP441 SD
#define STATUS_LED_PIN   2   // onboard blue LED, flashes on every detected clap

// ================= Audio =================
#define SAMPLE_RATE    16000
#define BLOCK_SAMPLES    256  // 16 ms of audio per block

// ================= Clap detection =================
// Levels are on a 16-bit scale (0-32767).
// Turn on DEBUG_LEVELS, open Serial Monitor at 115200, clap, and adjust.
#define CLAP_ABS_THRESHOLD  3000  // a clap must be at least this loud
#define CLAP_FLOOR_RATIO     6.0  // ...and this many times louder than background noise
#define CLAP_MAX_MS          200  // a clap must fade within this long (rejects music, talking, long noises)
#define QUIET_BEFORE_MS      500  // silence required before the first clap of a pattern

// ================= Clap pattern =================
// Relative gaps between claps. Only the ratios matter, so fast or slow both match.
//   {2, 1}    = clap ..... clap-clap   (default)
//   {1, 1}    = three even claps
//   {1, 1, 2} = clap-clap-clap ..... clap
static const float CLAP_PATTERN[] = {2.0, 1.0};

#define PATTERN_TOLERANCE 0.25   // +/-25% timing slack per gap
#define MIN_GAP_MS         120   // claps closer than this are treated as echoes
#define MAX_GAP_MS        1500   // longest allowed gap between claps in a pattern
#define PATTERN_END_MS    1700   // silence after the last clap that ends a pattern (keep > MAX_GAP_MS)
#define COOLDOWN_MS      10000   // ignore further matches for this long after triggering

// ================= Debug =================
#define DEBUG_LEVELS 0   // 1 = print loudness + noise floor ~5x per second for tuning
