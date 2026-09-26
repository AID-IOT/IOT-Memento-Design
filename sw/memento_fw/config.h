/*
 * memento — user configuration
 *
 * Edit the two WiFi lines below, then upload. Nothing else in here has to
 * change for the board to work.
 */
#ifndef MEMENTO_CONFIG_H
#define MEMENTO_CONFIG_H

// ---------------------------------------------------------------- WiFi ----
// Put your network name and password here.
#define WIFI_SSID       "YOUR_WIFI_NAME"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"

// How long to wait for the network before giving up, in milliseconds.
#define WIFI_TIMEOUT_MS 20000

// If the network above cannot be joined, the board starts its own hotspot
// with these credentials so the page is still reachable.
// An open hotspot is possible by setting AP_PASSWORD to "" — otherwise the
// password must be at least 8 characters.
#define AP_SSID         "memento-setup"
#define AP_PASSWORD     "memento123"

// Reachable as http://memento.local/ on networks that support mDNS.
#define MDNS_HOSTNAME   "memento"

// ------------------------------------------------------------- sensor ----
// Local sea-level pressure in hPa. The altitude reading is only as good as
// this number; 1013.25 is the standard-atmosphere default.
#define SEA_LEVEL_HPA   1013.25f

// -------------------------------------------------------------- LEDs -----
// Startup appearance. Modes: "solid", "rainbow", "breathe", "chase", "off".
#define LED_START_MODE       "rainbow"
#define LED_START_BRIGHTNESS 60     // 0-255
#define LED_START_R          0
#define LED_START_G          120
#define LED_START_B          255

#endif  // MEMENTO_CONFIG_H
