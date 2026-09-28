# Clap-to-Wake

A Tony Stark-inspired desk setup: clap a pattern and your PC powers on while the LED strip lights up. Built on two ESP32 boards, fully offline, no cloud services.

## Features

- **Clap pattern detection** with an I2S MEMS microphone, timing-tolerant so it matches whether you clap fast or slow
- **PC power-on** via Wake-on-LAN, or an optocoupler wired to the motherboard power switch header
- **Already-on check** so repeat claps never shut the PC down
- **LED control** through [WLED](https://kno.wled.ge/) running on a second ESP32
- **Separate "off" pattern** so lights never turn off by accident

## How It Works

```
 Claps --> INMP441 mic --> ESP32 #1 (clap detector)
                                      |
                |-------------------- v
                V                HTTP "on" command over Wi-Fi
   Wake-on-LAN packet /                          |
               |                                 V       
               V                    ESP32 #2 (WLED) -> LED strip
              PC                    

```

1. The mic listens continuously. Sharp, short spikes (fast attack, decay within ~100 ms) are logged as claps.
2. The gaps between claps are compared to stored patterns as ratios, with ±25% tolerance.
3. On a match, ESP32 #1 checks whether the PC is already on (ping). If not, it wakes it.
4. ESP32 #1 sends WLED an "on" command with the startup preset.

## Hardware

| Part | Qty | Notes |
|------|-----|-------|
| ESP32-WROOM-32 dev board | 2 | One for clap detection, one for WLED |
| INMP441 I2S microphone | 1 | MAX9814 analog mic also works |
| WS2812B / SK6812 LED strip (5V) | 1 | Any length |
| 5V power supply | 1 | Budget ~60 mA per LED at full white |
| 74AHCT125 level shifter | 1 | 3.3V -> 5V LED data |
| 1000 uF capacitor | 1 | Across strip power at the input |
| 330 ohm resistor | 1 | Inline on LED data |

**INMP441 -> ESP32**

| INMP441 | ESP32 |
|---------|-------|
| VDD | 3.3V |
| GND | GND |
| L/R | GND (left channel) |
| SCK | GPIO 14 |
| WS | GPIO 15 |
| SD | GPIO 32 |

All pins are configurable in `config.h`.

## Setup

### 1. PC (Wake-on-LAN)

- Enable **Wake on LAN** / **Power on by PCI-E** in the BIOS
- Windows Device Manager -> network adapter -> Power Management and Advanced tabs -> enable **Wake on Magic Packet**
- Use wired Ethernet
- Note the PC's MAC address and IP (`ipconfig /all`)

### 3. Clap detector

```bash
git clone https://github.com/YOUR_USERNAME/clap-to-wake.git
cd clap-to-wake
cp secrets.example.h secrets.h
```

Fill in `secrets.h`:

```cpp
#define WIFI_SSID     "your-network"
#define WIFI_PASSWORD "your-password"
#define PC_MAC        "AA:BB:CC:DD:EE:FF"
#define PC_IP         "192.168.1.50"
#define WLED_IP       "192.168.1.60"
```

Then build and upload with the Arduino IDE (board: **ESP32 Dev Module**) or PlatformIO.

> `secrets.h` is in `.gitignore`. Never commit it.

## Clap Patterns

| Pattern | Action |
|---------|--------|
| clap ... clap-clap | PC on |

Patterns and timing tolerance are set in `config.h`.

## Tuning

- **False triggers:** raise the spike threshold or require a longer quiet gap before the first clap
- **Missed claps:** lower the threshold or move the mic closer
- **Double-counted claps (echo):** increase the minimum gap between spikes (default 100 ms)
