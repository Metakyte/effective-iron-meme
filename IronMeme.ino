// Clap-to-Wake
// Listens for a clap pattern on an INMP441 mic and powers on the PC
// with Wake-on-LAN or an optocoupler on the power switch header.
//
// Board: ESP32 Dev Module (arduino-esp32 core 3.x)

#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESP_I2S.h>
#include "config.h"
#include "secrets.h"

// ---------- Globals ----------
I2SClass i2s;
WiFiUDP udp;

int32_t samples[BLOCK_SAMPLES];

float    noiseFloor   = 200;   // running average of background loudness
bool     inSpike      = false;
uint32_t spikeStart   = 0;
bool     spikeQuietOK = false; // was it quiet before this spike started?
uint32_t lastLoudEnd  = 0;     // when the last loud sound ended

const int MAX_CLAPS = 12;
uint32_t claps[MAX_CLAPS];
int      clapCount   = 0;
uint32_t lastTrigger = 0;
bool     everTriggered = false;

uint32_t ledOffAt = 0;
uint8_t  pcMac[6];

const int PATTERN_GAPS = sizeof(CLAP_PATTERN) / sizeof(CLAP_PATTERN[0]);

// ---------- Helpers ----------
bool parseMac(const char* s, uint8_t mac[6]) {
  unsigned int v[6];
  if (sscanf(s, "%x:%x:%x:%x:%x:%x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6 &&
      sscanf(s, "%x-%x-%x-%x-%x-%x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6) {
    return false;
  }
  for (int i = 0; i < 6; i++) mac[i] = (uint8_t)v[i];
  return true;
}

void flashLed(uint32_t ms) {
  digitalWrite(STATUS_LED_PIN, HIGH);
  ledOffAt = millis() + ms;
}

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  Serial.printf("Connecting to %s", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    delay(250);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nConnected, IP %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\nWi-Fi failed, will keep retrying in the background");
  }
}

// Loudest sample in one 16 ms block, scaled to 16-bit.
int32_t readBlockPeak() {
  size_t bytes = i2s.readBytes((char*)samples, sizeof(samples));
  int count = bytes / sizeof(int32_t);
  int32_t peak = 0;
  for (int i = 0; i < count; i++) {
    int32_t s = samples[i] >> 16;  // INMP441 sends 24-bit audio in a 32-bit slot
    if (s < 0) s = -s;
    if (s > peak) peak = s;
  }
  return peak;
}

// ---------- Powering on the PC ----------
void wakePC() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("No Wi-Fi, can't send Wake-on-LAN");
    return;
  }
  // Magic packet: 6 x 0xFF, then the MAC address 16 times
  uint8_t pkt[102];
  memset(pkt, 0xFF, 6);
  for (int i = 1; i <= 16; i++) memcpy(pkt + i * 6, pcMac, 6);

  IPAddress bcast = WiFi.broadcastIP();
  for (int i = 0; i < 3; i++) {  // send a few in case one gets dropped
    udp.beginPacket(bcast, 9);
    udp.write(pkt, sizeof(pkt));
    udp.endPacket();
    delay(50);
  }
  Serial.printf("Sent Wake-on-LAN to %s via %s\n", PC_MAC, bcast.toString().c_str());
}

// ---------- Pattern matching ----------
void evaluatePattern() {
  uint32_t now = millis();

  if (clapCount != PATTERN_GAPS + 1) {
    Serial.printf("Heard %d claps, pattern needs %d\n", clapCount, PATTERN_GAPS + 1);
    return;
  }

  float gaps[PATTERN_GAPS];
  float gapSum = 0, patternSum = 0;
  for (int i = 0; i < PATTERN_GAPS; i++) {
    gaps[i] = claps[i + 1] - claps[i];
    if (gaps[i] < MIN_GAP_MS || gaps[i] > MAX_GAP_MS) {
      Serial.printf("Gap %d out of range (%.0f ms)\n", i + 1, gaps[i]);
      return;
    }
    gapSum += gaps[i];
    patternSum += CLAP_PATTERN[i];
  }

  // Scale the pattern to however fast you clapped, then compare each gap
  float unit = gapSum / patternSum;
  for (int i = 0; i < PATTERN_GAPS; i++) {
    float expected = CLAP_PATTERN[i] * unit;
    Serial.printf("  gap %d: %.0f ms (expected ~%.0f)\n", i + 1, gaps[i], expected);
    if (fabs(gaps[i] - expected) > expected * PATTERN_TOLERANCE) {
      Serial.println("Rhythm didn't match");
      return;
    }
  }

  if (everTriggered && now - lastTrigger < COOLDOWN_MS) {
    Serial.println("Pattern matched, but still in cooldown");
    return;
  }

  Serial.println(">>> Pattern matched! Waking PC");
  lastTrigger = now;
  everTriggered = true;
  flashLed(1000);
  wakePC();
}

void registerClap(uint32_t t, bool quietBefore) {
  if (clapCount == 0 && !quietBefore) {
    Serial.println("Clap ignored (no quiet gap before it)");
    return;
  }
  if (clapCount > 0 && t - claps[clapCount - 1] < MIN_GAP_MS) return;  // echo
  if (clapCount >= MAX_CLAPS) return;

  claps[clapCount++] = t;
  flashLed(80);
  Serial.printf("Clap %d\n", clapCount);
}

// ---------- Setup / loop ----------
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nClap-to-Wake starting");

  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);

  if (!parseMac(PC_MAC, pcMac)) {
    Serial.println("PC_MAC in secrets.h isn't a valid MAC address");
  }

  connectWiFi();

  i2s.setPins(MIC_SCK_PIN, MIC_WS_PIN, -1, MIC_SD_PIN);
  if (!i2s.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_32BIT,
                 I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT)) {
    Serial.println("Mic init failed, check wiring");
    while (true) delay(1000);
  }

  // Let the mic settle and learn the room's background noise
  uint32_t t0 = millis();
  while (millis() - t0 < 1500) {
    int32_t p = readBlockPeak();
    noiseFloor = noiseFloor * 0.9 + p * 0.1;
  }
  lastLoudEnd = millis() - QUIET_BEFORE_MS;
  Serial.printf("Ready. Noise floor: %.0f\n", noiseFloor);
}

void loop() {
  int32_t  peak = readBlockPeak();
  uint32_t now  = millis();

  int32_t threshold = max((int32_t)CLAP_ABS_THRESHOLD, (int32_t)(noiseFloor * CLAP_FLOOR_RATIO));

  if (!inSpike) {
    if (peak > threshold) {
      inSpike = true;
      spikeStart = now;
      spikeQuietOK = (now - lastLoudEnd) >= QUIET_BEFORE_MS;
    } else {
      noiseFloor = noiseFloor * 0.995 + peak * 0.005;  // slowly track background noise
    }
  } else if (peak < threshold / 2) {
    // Sound faded: it was a clap if it was short enough
    inSpike = false;
    lastLoudEnd = now;
    if (now - spikeStart <= CLAP_MAX_MS) {
      registerClap(spikeStart, spikeQuietOK);
    } else {
      clapCount = 0;  // long noise, not a clap: start over
    }
  }

  // A pattern ends after a stretch with no new claps
  if (clapCount > 0 && !inSpike && now - claps[clapCount - 1] > PATTERN_END_MS) {
    evaluatePattern();
    clapCount = 0;
  }

  if (ledOffAt && now >= ledOffAt) {
    digitalWrite(STATUS_LED_PIN, LOW);
    ledOffAt = 0;
  }

  static uint32_t lastWiFiCheck = 0;
  if (now - lastWiFiCheck > 30000) {
    lastWiFiCheck = now;
    if (WiFi.status() != WL_CONNECTED) WiFi.reconnect();
  }

#if DEBUG_LEVELS
  static uint32_t lastPrint = 0;
  if (now - lastPrint > 200) {
    lastPrint = now;
    Serial.printf("level %5ld  floor %5.0f  threshold %5ld\n", (long)peak, noiseFloor, (long)threshold);
  }
#endif
}
