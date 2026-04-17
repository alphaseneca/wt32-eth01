# WT32-ETH01 — ESP32 Ethernet Board

ESP32 + LAN8720A onboard Ethernet PHY (10/100 Mbps).  
Uses PlatformIO with Arduino framework.

## Features
- **Ethernet** via built-in `ETH` library (RMII, LAN8720A)
- **MQTT** client with auto-reconnect (PubSubClient)
- **Web Server** dashboard showing IP, MAC, link speed, uptime, heap

## Quick Start

1. Wire USB-TTL adapter: `TX→RX0`, `RX→TX0`, `GND→GND`, `5V→5V`
2. Jumper `IO0` to `GND` for bootloader mode
3. `pio run --target upload`
4. Remove IO0 jumper, reset board
5. Open serial monitor: `pio device monitor`

## Key Pins (Internal RMII — do not use)
| Function | GPIO |
|:---|:---:|
| MDC | 23 |
| MDIO | 18 |
| REF_CLK | 0 |
| OSC Enable | 16 |
| PHY Addr | 1 |

## Reference

Based on [egnor/wt32-eth01](https://github.com/egnor/wt32-eth01) — comprehensive WT32-ETH01 documentation and code examples.
