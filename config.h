#pragma once

// ── WiFi ─────────────────────────────────────────────────────────
#define WIFI_SSID              "YOUR_SSID"
#define WIFI_PASSWORD          "YOUR_PASSWORD"

// ── NTP (ajuste le fuseau horaire) ───────────────────────────────
#define NTP_SERVER              "pool.ntp.org"
#define NTP_GMT_OFFSET_SEC      (-5 * 3600L)   // EST
#define NTP_DAYLIGHT_OFFSET_SEC (3600)

// ── GPIO (evite les pins 6-11 reservés au flash SPI) ─────────────
// NOTE: sur ESP32-WROVER (avec PSRAM), GPIO 16 et 17 sont utilises
// par la PSRAM. Utilise des pins libres (ex: 25, 26, 27) sur WROVER.
#define PIN_FAN     4
#define PIN_DEHUM   16
#define PIN_HEATER  17

// ── I2C ──────────────────────────────────────────────────────────
#define I2C_SDA 21
#define I2C_SCL 22

// ── BME280 ───────────────────────────────────────────────────────
#define BME280_INT_ADDR 0x77   // capteur interieur
#define BME280_EXT_ADDR 0x76   // capteur exterieur

// ── Boucle de controle ───────────────────────────────────────────
#define CONTROL_INTERVAL_MS (60UL * 1000UL)    // 1 minute
