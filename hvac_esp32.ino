/*
 * HVAC Hangar — Port ESP32
 * Port Arduino du script hvac.py (Raspberry Pi) pour ESP32.
 *
 * Equivalent fonctionnel:
 *   hvac.py --cron  ->  boucle de controle toutes les 60s (loop)
 *   hvac.py         ->  dashboard web sur port 80 (IP affichee en Serial)
 *   hvac.py heater  ->  bouton ON/OFF dans le dashboard web
 *
 * Librairies requises (Arduino Library Manager):
 *   - Adafruit BME280 Library   (Adafruit)
 *   - Adafruit Unified Sensor   (Adafruit)
 *   - ArduinoJson               (Benoit Blanchon)
 *
 * Incluses avec le core ESP32:
 *   - WebServer, Preferences, WiFi, Wire
 *
 * Connexion BME280 (I2C):
 *   SDA -> GPIO 21
 *   SCL -> GPIO 22
 *   Capteur interieur : adresse 0x77
 *   Capteur exterieur : adresse 0x76
 */

#include "config.h"
#include "hvac_types.h"
#include "hvac_control.h"
#include "hvac_storage.h"
#include "hvac_web.h"

#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <time.h>

// ── Globaux (accessibles depuis hvac_web.cpp via pointeurs) ──────
HvacConfig g_config;
HvacState  g_state;
Sensors    g_sensors;
GpioState  g_gpio;
Decisions  g_decisions;

static Adafruit_BME280 bme_int, bme_ext;
static bool            bme_ok = false;
static unsigned long   last_control_ms = 0;

// ── GPIO ─────────────────────────────────────────────────────────
static void gpio_init() {
    pinMode(PIN_FAN,    OUTPUT);
    pinMode(PIN_DEHUM,  OUTPUT);
    pinMode(PIN_HEATER, OUTPUT);
    digitalWrite(PIN_FAN,    LOW);
    digitalWrite(PIN_DEHUM,  LOW);
    digitalWrite(PIN_HEATER, LOW);
}

static void gpio_read() {
    g_gpio.fan    = digitalRead(PIN_FAN);
    g_gpio.dehum  = digitalRead(PIN_DEHUM);
    g_gpio.heater = digitalRead(PIN_HEATER);
}

static void gpio_apply(bool fan, bool dehum, bool heater) {
    digitalWrite(PIN_FAN,    fan    ? HIGH : LOW);
    digitalWrite(PIN_DEHUM,  dehum  ? HIGH : LOW);
    digitalWrite(PIN_HEATER, heater ? HIGH : LOW);
    g_gpio.fan    = fan;
    g_gpio.dehum  = dehum;
    g_gpio.heater = heater;
}

// ── Capteurs ─────────────────────────────────────────────────────
static bool sensors_init() {
    Wire.begin(I2C_SDA, I2C_SCL);
    bool ok = true;
    if (!bme_int.begin(BME280_INT_ADDR, &Wire)) {
        Serial.println("ERREUR: BME280 interieur non detecte (0x77)");
        ok = false;
    }
    if (!bme_ext.begin(BME280_EXT_ADDR, &Wire)) {
        Serial.println("ERREUR: BME280 exterieur non detecte (0x76)");
        ok = false;
    }
    return ok;
}

static bool sensors_read() {
    float corr      = g_config.humidity_correction;
    g_sensors.H_int = bme_int.readHumidity()    / corr;
    g_sensors.T_int = bme_int.readTemperature();
    g_sensors.H_ext = bme_ext.readHumidity()    / corr;
    g_sensors.T_ext = bme_ext.readTemperature();
    g_sensors.valid = !isnan(g_sensors.H_int) && !isnan(g_sensors.T_int)
                   && !isnan(g_sensors.H_ext) && !isnan(g_sensors.T_ext);
    return g_sensors.valid;
}

// ── Boucle de controle (equivalent au mode --cron) ───────────────
static void control_run() {
    // Garde NTP : sans heure valide, le calcul predictif et les timers
    // se basent sur des deltas erratiques et l'etat sauvegarde devient corrompu.
    if (time(nullptr) < 100000) {
        Serial.println("[HVAC] NTP non synchronise, attente");
        return;
    }
    if (!sensors_read()) {
        Serial.println("[HVAC] Erreur lecture capteurs");
        return;
    }
    gpio_read();

    Serial.printf("[HVAC] Int: %.1fC %.1f%%  |  Ext: %.1fC %.1f%%\n",
        g_sensors.T_int, g_sensors.H_int, g_sensors.T_ext, g_sensors.H_ext);

    g_decisions = compute(g_sensors, g_config, g_state, g_gpio);

    // Overrides manuels fan et dehum (identique a run_cron() Python)
    bool want_fan   = g_decisions.fan;
    bool want_dehum = g_decisions.dehum;
    if (g_state.manual_fan >= 0) {
        want_fan = (g_state.manual_fan == 1);
        g_decisions.fan_reason = String("Manuel ") + (want_fan ? "ON" : "OFF");
        g_decisions.fan = want_fan;
    }
    if (g_state.manual_dehum >= 0) {
        want_dehum = (g_state.manual_dehum == 1);
        g_decisions.dehum_reason = String("Manuel ") + (want_dehum ? "ON" : "OFF");
        g_decisions.dehum = want_dehum;
    }

    gpio_apply(want_fan, want_dehum, g_decisions.heater);

    Serial.printf("[HVAC] Fan:%s(%s)  Dehum:%s(%s)  Heat:%s(%s)\n",
        g_gpio.fan    ? "ON" : "OFF", g_decisions.fan_reason.c_str(),
        g_gpio.dehum  ? "ON" : "OFF", g_decisions.dehum_reason.c_str(),
        g_gpio.heater ? "ON" : "OFF", g_decisions.heater_reason.c_str());

    storage_save_state(g_state);
}

// ── Setup ─────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== HVAC Hangar ESP32 ===");

    gpio_init();
    storage_load_config(g_config);
    storage_load_state(g_state);

    bme_ok = sensors_init();
    if (!bme_ok)
        Serial.println("AVERTISSEMENT: capteurs BME280 non initialises — controle suspendu");

    // Connexion WiFi (avec auto-reconnect)
    Serial.printf("Connexion WiFi: %s\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    for (int i = 0; i < 30 && WiFi.status() != WL_CONNECTED; i++) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("WiFi connecte: http://%s\n", WiFi.localIP().toString().c_str());
        configTime(NTP_GMT_OFFSET_SEC, NTP_DAYLIGHT_OFFSET_SEC, NTP_SERVER);
        // Attente de la sync NTP (max 5s)
        time_t t = 0;
        for (int i = 0; i < 10 && t < 100000; i++) { delay(500); t = time(nullptr); }
        Serial.println("NTP synchronise");
    } else {
        Serial.println("WiFi non connecte — mode offline (web indisponible)");
    }

    web_begin(g_config, g_state, g_sensors, g_gpio, g_decisions);

    // Premiere lecture immediate (control_run() s'auto-protege si NTP pas sync)
    if (bme_ok) {
        control_run();
        last_control_ms = millis();
    }
}

// ── Loop ──────────────────────────────────────────────────────────
void loop() {
    web_handle();

    if (bme_ok && (millis() - last_control_ms >= CONTROL_INTERVAL_MS)) {
        control_run();
        last_control_ms = millis();
    }
}
