/* ═══════════════════════════════════════════════════════════════════
 *  HVAC Hangar — Port ESP32
 *  Port Arduino du script hvac.py (Raspberry Pi) pour ESP32.
 * ═══════════════════════════════════════════════════════════════════
 *
 *  EQUIVALENT FONCTIONNEL hvac.py -> ESP32
 *    hvac.py --cron   ->  boucle de controle toutes les 60s (loop)
 *    hvac.py          ->  dashboard web sur port 80
 *    hvac.py heater   ->  boutons ON/OFF dans le dashboard web
 *
 * ───────────────────────────────────────────────────────────────────
 *  1. CORE BOARD A INSTALLER (Arduino IDE -> Boards Manager)
 * ───────────────────────────────────────────────────────────────────
 *    Package : "esp32" by Espressif Systems
 *    URL JSON: https://espressif.github.io/arduino-esp32/package_esp32_index.json
 *    Version testee : 3.3.8
 *
 * ───────────────────────────────────────────────────────────────────
 *  2. LIBRAIRIES A INSTALLER (Arduino IDE -> Library Manager)
 * ───────────────────────────────────────────────────────────────────
 *    Adafruit BME280 Library      v2.3.0   (Adafruit)
 *    Adafruit Unified Sensor      v1.1.15  (Adafruit)  [dependance BME280]
 *    Adafruit BusIO               v1.17.4  (Adafruit)  [dependance BME280]
 *    ArduinoJson                  v7.4.3   (Benoit Blanchon)
 *
 *    INCLUSES avec le core ESP32 (rien a installer) :
 *      WiFi.h, WebServer.h, Preferences.h, Wire.h, time.h
 *
 * ───────────────────────────────────────────────────────────────────
 *  3. PARAMETRES DE COMPILATION (Arduino IDE -> Tools)
 * ───────────────────────────────────────────────────────────────────
 *    Board              : "ESP32 Dev Module"  (FQBN esp32:esp32:esp32)
 *    Upload Speed       : 921600
 *    CPU Frequency      : 240MHz (WiFi/BT)
 *    Flash Frequency    : 80MHz
 *    Flash Mode         : QIO
 *    Flash Size         : 4MB (32Mb)
 *    Partition Scheme   : Default 4MB with spiffs (1.2MB APP / 1.5MB SPIFFS)
 *    Core Debug Level   : None
 *    PSRAM              : Disabled       (Enabled UNIQUEMENT si module WROVER)
 *
 *    Empreinte actuelle : 78% flash (~1.03 MB), 15% RAM (~50 KB)
 *
 * ───────────────────────────────────────────────────────────────────
 *  4. CABLAGE
 * ───────────────────────────────────────────────────────────────────
 *    BME280 interieur  (I2C 0x77)  ┐
 *    BME280 exterieur  (I2C 0x76)  ┴── SDA -> GPIO 21
 *                                      SCL -> GPIO 22
 *                                      VCC -> 3.3V
 *                                      GND -> GND
 *
 *    Relais ventilateur            -> GPIO 4   (PIN_FAN)
 *    Relais deshumidificateur      -> GPIO 16  (PIN_DEHUM)
 *    Relais chauffage              -> GPIO 17  (PIN_HEATER)
 *
 *    ATTENTION : sur ESP32-WROVER (avec PSRAM), GPIO 16 et 17 sont
 *    reserves a la PSRAM. Reaffecter PIN_DEHUM/PIN_HEATER vers
 *    GPIO 25/26/27 dans config.h si tu utilises un WROVER.
 *
 * ───────────────────────────────────────────────────────────────────
 *  5. CONFIGURATION AVANT FLASH (config.h)
 * ───────────────────────────────────────────────────────────────────
 *    WIFI_SSID / WIFI_PASSWORD     -> identifiants du reseau WiFi
 *    NTP_GMT_OFFSET_SEC            -> fuseau horaire (defaut: -5h EST)
 *    NTP_DAYLIGHT_OFFSET_SEC       -> heure d'ete (defaut: 3600)
 *    PIN_FAN / PIN_DEHUM / PIN_HEATER  -> reaffecter selon cablage
 *
 * ═══════════════════════════════════════════════════════════════════
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
