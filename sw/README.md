# memento — firmware

Firmware for the memento board: a **Seeed XIAO ESP32C3** driving **4× WS2812B**
LEDs and reading a **GY-BMP280-3.3** pressure/temperature module. It joins your
WiFi and serves a web page with the live readings and LED controls.

The same folder works two ways — open it in the Arduino IDE, or build it with
PlatformIO. Nothing needs converting.

```
sw/
├── platformio.ini        PlatformIO config (points src_dir at the sketch folder)
└── memento_fw/
    ├── memento_fw.ino    the sketch
    └── config.h          WiFi name/password and startup settings — edit this
```

## 1. Set your WiFi

Open `memento_fw/config.h` and change these two lines:

```c
#define WIFI_SSID       "YOUR_WIFI_NAME"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"
```

If the board cannot join that network it starts its own hotspot instead
(`memento-setup`, password `memento123`), so the page is still reachable.
Both are configurable in the same file.

## 2a. Arduino IDE

1. **File → Preferences → Additional boards manager URLs**, add:
   `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
2. **Tools → Board → Boards Manager**, install **esp32** by Espressif Systems.
3. **Tools → Board → ESP32 Arduino → XIAO_ESP32C3**.
4. **Tools → Manage Libraries**, install:
   - Adafruit NeoPixel
   - Adafruit BMP280 Library
   - Adafruit Unified Sensor
   - Adafruit BusIO
5. Open `memento_fw/memento_fw.ino`, edit `config.h`, upload.

## 2b. PlatformIO

From this `sw/` folder — libraries download automatically:

```
pio run              # build
pio run -t upload    # build and flash
pio device monitor   # serial log, 115200
```

## 3. Open the page

After upload, open the serial monitor at **115200**. It prints the address:

```
Joining "YOUR_WIFI_NAME" ....
Connected. Open http://192.168.1.42/  (or http://memento.local/)
BMP280 found at 0x76
HTTP server up on port 80
```

The page shows temperature, pressure and altitude (refreshing every 2 s) and
lets you set the LED mode, colour, brightness and animation speed.

## Pin map

Taken from the schematic (`../hw/memento.kicad_sch`), not assumed:

| Signal | XIAO pin | GPIO | Goes to |
|---|---|---|---|
| WS2812B data | D0 | **2** | R1 → D2.DIN → D3 → D4 → D5 (4 LEDs) |
| I²C SDA | D4 | **6** | U2 (BMP280) SDA |
| I²C SCL | D5 | **7** | U2 (BMP280) SCL |
| Power | 5V / GND | — | LEDs and sensor module |

## HTTP endpoints

| Route | Purpose |
|---|---|
| `GET /` | the control page |
| `GET /api/status` | JSON: sensor readings, LED state, network, uptime |
| `GET /api/led` | set LEDs — `mode`, `r`, `g`, `b`, `brightness`, `speed` |

`mode` is one of `solid`, `rainbow`, `breathe`, `chase`, `off`.
Example:

```
curl "http://memento.local/api/led?mode=solid&r=255&g=80&b=0&brightness=120"
```

## Notes on the hardware

- **The BMP280 module's VCC is wired to +5V on this board.** The GY-BMP280-**3.3**
  variant is a 3.3 V part with no on-board regulator, and the BMP280 is not 5 V
  tolerant. Check your module before powering it: if it has no regulator, move
  VCC to the XIAO's 3V3 pin. The firmware cannot protect against this.
- **CSB (pin 5) and SDO (pin 6) of the module are left unconnected.** Most
  GY-BMP280 boards pull CSB high (selects I²C) and SDO low (address `0x76`)
  with on-board resistors, so this usually works. The firmware probes `0x76`
  then `0x77` and keeps retrying every 5 s, so a late or intermittent sensor
  recovers without a reset. If it is never detected, tie CSB to VCC.
- The WS2812B chain runs at 5 V while the XIAO's data output is 3.3 V. That is
  below the 0.7 × VDD input threshold in the datasheet. It commonly works, and
  R1 sits in series on the data line, but a level shifter is the in-spec fix if
  the first LED misbehaves.
- GPIO2 is a strapping pin on the ESP32-C3. The WS2812B data input is
  high-impedance so it does not disturb boot mode.
