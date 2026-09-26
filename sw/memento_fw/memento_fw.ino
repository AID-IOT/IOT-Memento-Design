/*
 * memento — AICTE IDEA Lab, Poornima College of Engineering
 *
 * Seeed XIAO ESP32C3 + 4x WS2812B + GY-BMP280-3.3 module.
 * Joins your WiFi and serves a page showing the sensor readings and a
 * control for the LEDs.
 *
 * Pin map — read from the schematic (hw/memento.kicad_sch), not guessed:
 *   WS2812B data  XIAO D0 = GPIO2 -> R1 -> D2.DIN -> D3 -> D4 -> D5
 *   I2C SDA       XIAO D4 = GPIO6 -> U2 (BMP280) SDA
 *   I2C SCL       XIAO D5 = GPIO7 -> U2 (BMP280) SCL
 *   Power         +5V and GND from the XIAO
 *
 * Arduino IDE: open this file, pick board "XIAO_ESP32C3", install the
 * libraries listed in ../README.md, edit config.h, upload.
 * PlatformIO:   from the sw/ folder run `pio run -t upload`.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_BMP280.h>

#include "config.h"
#include "index_html.h"

// ------------------------------------------------------------- hardware ---
static const uint8_t  PIN_LED_DATA = 2;  // XIAO D0 / GPIO2
static const uint8_t  PIN_I2C_SDA  = 6;  // XIAO D4 / GPIO6
static const uint8_t  PIN_I2C_SCL  = 7;  // XIAO D5 / GPIO7
static const uint16_t NUM_LEDS     = 4;  // D2 -> D3 -> D4 -> D5

Adafruit_NeoPixel strip(NUM_LEDS, PIN_LED_DATA, NEO_GRB + NEO_KHZ800);
Adafruit_BMP280 bmp;
WebServer server(80);

static bool    bmpOk   = false;
static uint8_t bmpAddr = 0x00;

// ------------------------------------------------------------ LED state ---
enum LedMode { MODE_SOLID, MODE_RAINBOW, MODE_BREATHE, MODE_CHASE, MODE_OFF };

static LedMode  ledMode       = MODE_RAINBOW;
static uint8_t  ledR          = LED_START_R;
static uint8_t  ledG          = LED_START_G;
static uint8_t  ledB          = LED_START_B;
static uint8_t  ledBrightness = LED_START_BRIGHTNESS;
static uint16_t ledSpeed      = 25;  // ms per animation step

static const char *modeName(LedMode m) {
  switch (m) {
    case MODE_SOLID:   return "solid";
    case MODE_RAINBOW: return "rainbow";
    case MODE_BREATHE: return "breathe";
    case MODE_CHASE:   return "chase";
    default:           return "off";
  }
}

static LedMode modeFromName(const String &s) {
  if (s == "solid")   return MODE_SOLID;
  if (s == "rainbow") return MODE_RAINBOW;
  if (s == "breathe") return MODE_BREATHE;
  if (s == "chase")   return MODE_CHASE;
  return MODE_OFF;
}

// ------------------------------------------------------------ animation ---
static void renderLeds() {
  static uint32_t last = 0;
  static uint16_t step = 0;
  uint32_t now = millis();
  if (now - last < ledSpeed) return;
  last = now;
  step++;

  switch (ledMode) {
    case MODE_OFF:
      strip.setBrightness(0);
      strip.clear();
      break;

    case MODE_SOLID:
      strip.setBrightness(ledBrightness);
      for (uint16_t i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, strip.Color(ledR, ledG, ledB));
      }
      break;

    case MODE_RAINBOW: {
      strip.setBrightness(ledBrightness);
      for (uint16_t i = 0; i < NUM_LEDS; i++) {
        uint16_t hue = (uint16_t)((uint32_t)step * 256UL +
                                  (uint32_t)i * 65536UL / NUM_LEDS);
        strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(hue)));
      }
      break;
    }

    case MODE_BREATHE: {
      // triangle wave 0..255..0, scaled by the brightness setting
      uint8_t phase = (uint8_t)(step & 0xFF);
      uint8_t wave  = phase < 128 ? (uint8_t)(phase * 2)
                                  : (uint8_t)((255 - phase) * 2);
      strip.setBrightness((uint8_t)((uint16_t)wave * ledBrightness / 255));
      for (uint16_t i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, strip.Color(ledR, ledG, ledB));
      }
      break;
    }

    case MODE_CHASE: {
      strip.setBrightness(ledBrightness);
      uint16_t head = (uint16_t)((step / 4) % NUM_LEDS);
      for (uint16_t i = 0; i < NUM_LEDS; i++) {
        if (i == head) {
          strip.setPixelColor(i, strip.Color(ledR, ledG, ledB));
        } else {
          strip.setPixelColor(i, strip.Color(ledR / 12, ledG / 12, ledB / 12));
        }
      }
      break;
    }
  }
  strip.show();
}

// --------------------------------------------------------------- sensor ---
static bool readSensor(float &tempC, float &hPa, float &altM) {
  if (!bmpOk) return false;
  tempC = bmp.readTemperature();
  hPa   = bmp.readPressure() / 100.0f;
  altM  = bmp.readAltitude(SEA_LEVEL_HPA);
  // a disconnected BMP280 reads back as NaN or a constant 0
  return !isnan(tempC) && !isnan(hPa) && hPa > 1.0f;
}

static bool initSensor() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(100000);
  const uint8_t addrs[] = {0x76, 0x77};
  for (uint8_t i = 0; i < 2; i++) {
    if (bmp.begin(addrs[i])) {
      bmpAddr = addrs[i];
      bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                      Adafruit_BMP280::SAMPLING_X2,
                      Adafruit_BMP280::SAMPLING_X16,
                      Adafruit_BMP280::FILTER_X16,
                      Adafruit_BMP280::STANDBY_MS_500);
      Serial.printf("BMP280 found at 0x%02X\n", bmpAddr);
      return true;
    }
  }
  Serial.println("BMP280 NOT found on I2C (checked 0x76 and 0x77)");
  return false;
}

// -------------------------------------------------------------- handlers --
static void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

static void handleStatus() {
  float t = 0, p = 0, a = 0;
  bool ok  = readSensor(t, p, a);
  bool sta = (WiFi.status() == WL_CONNECTED);

  String ip = sta ? WiFi.localIP().toString() : WiFi.softAPIP().toString();

  char buf[420];
  snprintf(buf, sizeof(buf),
           "{\"sensor\":{\"ok\":%s,\"address\":%u,"
           "\"temperature_c\":%.2f,\"pressure_hpa\":%.2f,\"altitude_m\":%.2f},"
           "\"led\":{\"mode\":\"%s\",\"r\":%u,\"g\":%u,\"b\":%u,"
           "\"brightness\":%u,\"speed\":%u},"
           "\"net\":{\"mode\":\"%s\",\"ip\":\"%s\",\"rssi\":%d},"
           "\"uptime_s\":%lu}",
           ok ? "true" : "false", (unsigned)bmpAddr,
           ok ? t : 0.0f, ok ? p : 0.0f, ok ? a : 0.0f,
           modeName(ledMode), (unsigned)ledR, (unsigned)ledG, (unsigned)ledB,
           (unsigned)ledBrightness, (unsigned)ledSpeed,
           sta ? "wifi" : "hotspot", ip.c_str(),
           sta ? (int)WiFi.RSSI() : 0,
           (unsigned long)(millis() / 1000));
  server.send(200, "application/json", buf);
}

static uint8_t argByte(const char *name, uint8_t fallback) {
  if (!server.hasArg(name)) return fallback;
  long v = server.arg(name).toInt();
  if (v < 0) v = 0;
  if (v > 255) v = 255;
  return (uint8_t)v;
}

static void handleLed() {
  if (server.hasArg("mode")) ledMode = modeFromName(server.arg("mode"));
  ledR = argByte("r", ledR);
  ledG = argByte("g", ledG);
  ledB = argByte("b", ledB);
  ledBrightness = argByte("brightness", ledBrightness);
  if (server.hasArg("speed")) {
    long v = server.arg("speed").toInt();
    if (v < 5)   v = 5;
    if (v > 200) v = 200;
    ledSpeed = (uint16_t)v;
  }
  server.send(200, "application/json", "{\"ok\":true}");
}

static void handleNotFound() {
  server.send(404, "text/plain", "not found");
}

// ---------------------------------------------------------------- setup ---
static void startNetwork() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(MDNS_HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("Joining \"%s\" ", WIFI_SSID);

  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < WIFI_TIMEOUT_MS) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("Connected. Open http://%s/  (or http://%s.local/)\n",
                  WiFi.localIP().toString().c_str(), MDNS_HOSTNAME);
  } else {
    Serial.println("Could not join that network - starting hotspot instead.");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, strlen(AP_PASSWORD) >= 8 ? AP_PASSWORD : NULL);
    Serial.printf("Hotspot \"%s\" -> open http://%s/\n",
                  AP_SSID, WiFi.softAPIP().toString().c_str());
  }

  if (MDNS.begin(MDNS_HOSTNAME)) MDNS.addService("http", "tcp", 80);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nmemento starting");

  strip.begin();
  strip.setBrightness(ledBrightness);
  strip.clear();
  strip.show();
  ledMode = modeFromName(String(LED_START_MODE));

  bmpOk = initSensor();

  startNetwork();

  server.on("/", handleRoot);
  server.on("/api/status", handleStatus);
  server.on("/api/led", handleLed);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server up on port 80");
}

void loop() {
  server.handleClient();
  renderLeds();

  // if the sensor was missing at boot, retry occasionally so a late or loose
  // connection still comes up without a power cycle
  static uint32_t lastTry = 0;
  if (!bmpOk && millis() - lastTry > 5000) {
    lastTry = millis();
    bmpOk = initSensor();
  }
}
