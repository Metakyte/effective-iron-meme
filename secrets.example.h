#pragma once

// Copy this file to secrets.h and fill in your values.
// secrets.h is in .gitignore, so it never gets committed if you're cloning this.

#define WIFI_SSID     "your-network"
#define WIFI_PASSWORD "your-password"

// PC's MAC address, from `ipconfig /all` (Physical Address). Colons or dashes both work.
#define PC_MAC        "AA:BB:CC:DD:EE:FF"

// PC's IP address. Only used by the optocoupler mode's "is the PC on?" ping.
// Give the PC a DHCP reservation in your router so this never changes.
#define PC_IP         "192.168.1.50"
