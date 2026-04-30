#include "hvac_control.h"
#include <math.h>
#include <time.h>

Decisions compute(const Sensors& sensors, const HvacConfig& cfg,
                  HvacState& state, const GpioState& gpio) {
    Decisions d;
    time_t now = time(nullptr);

    float H_int = sensors.H_int, T_int = sensors.T_int;
    float H_ext = sensors.H_ext, T_ext = sensors.T_ext;

    // ── Prediction (taux de montee d'humidite exterieure) ────────
    float rate_ext = 0.0f;
    float seuil_on = cfg.humidity_on;

    if (state.has_H_ext_prev && state.t_prev > 0) {
        float delta_h = H_ext - state.H_ext_prev;
        float delta_t = (float)(now - state.t_prev) / 3600.0f;
        if (delta_t > 0.0f)
            rate_ext = fmaxf(0.0f, delta_h / delta_t);
        if (rate_ext >= cfg.rate_min) {
            if (H_ext + rate_ext > H_int) {
                seuil_on = cfg.humidity_on - cfg.k_predictive * rate_ext;
                seuil_on = fmaxf(seuil_on, cfg.humidity_on - cfg.humidity_hysteresis);
            }
        } else {
            rate_ext = 0.0f;
        }
    }
    float seuil_off = seuil_on - cfg.humidity_hysteresis;
    d.rate_ext = rate_ext;
    d.seuil_on = seuil_on;
    d.seuil_off = seuil_off;

    // ── Ventilateur ──────────────────────────────────────────────
    d.fan = (H_ext < cfg.humidity_fan_max && T_ext > T_int + cfg.temp_fan_delta);
    if (d.fan) {
        d.fan_reason = "Air ext sec et chaud";
    } else {
        String parts;
        if (H_ext >= cfg.humidity_fan_max)
            parts += "Air ext humide " + String((int)H_ext) + "%";
        if (T_ext <= T_int + cfg.temp_fan_delta) {
            if (parts.length()) parts += ", ";
            parts += "Air ext froid " + String(T_ext, 1) + "C";
        }
        d.fan_reason = parts;
    }

    // ── Deshumidificateur ────────────────────────────────────────
    bool want_dehum = false;
    time_t new_dehum_on_since = state.dehum_on_since;
    d.heater_clim = false;

    if (T_int < cfg.temp_min_dehum) {
        // Trop froid pour deshumidifier (risque givre)
        want_dehum = false;
        d.dehum_reason = "Givre! " + String(T_int, 1) + "C<" + String(cfg.temp_min_dehum, 0) + "C";
        new_dehum_on_since = 0;
        if (H_int >= seuil_on) {
            d.heater_clim = true;  // active le chauffage pour monter la temp
        }
    } else {
        if (!gpio.dehum) {
            // Deshumidificateur eteint
            if (H_int >= seuil_on) {
                want_dehum = true;
                d.dehum_reason = "Humide " + String((int)H_int) + "%>=" + String((int)seuil_on) + "%";
                new_dehum_on_since = now;
            } else {
                d.dehum_reason = "Hum. ok " + String((int)H_int) + "%";
            }
        } else {
            // Deshumidificateur allume — respecte le cycle minimum
            long min_sec = (long)cfg.min_run_dehum_min * 60L;
            if (state.dehum_on_since > 0 && (now - state.dehum_on_since) < min_sec) {
                want_dehum = true;
                int mins = (int)((now - state.dehum_on_since) / 60);
                d.dehum_reason = "Cycle min " + String(mins) + "min";
            } else {
                float soff = cfg.humidity_on - cfg.humidity_hysteresis;
                if (H_int < soff) {
                    want_dehum = false;
                    d.dehum_reason = "Sec " + String((int)H_int) + "%<" + String((int)soff) + "%";
                    new_dehum_on_since = 0;
                } else {
                    want_dehum = true;
                    d.dehum_reason = "Sechage " + String((int)H_int) + "%";
                }
            }
        }
    }
    d.dehum = want_dehum;

    // ── Chauffage ────────────────────────────────────────────────
    time_t new_heater_off_at = state.heater_manual_off_at;
    bool manual_active = (state.heater_manual_off_at > 0 && now < state.heater_manual_off_at);
    bool heater_active = manual_active || d.heater_clim;

    // Le mode CLIM pose un timer de 10 min s'il n'y a pas de timer manuel
    if (d.heater_clim && !manual_active)
        new_heater_off_at = now + 10L * 60L;

    bool want_heater = false;
    if (heater_active) {
        if (T_int < cfg.temp_heater_low) {
            want_heater = true;
            d.heater_reason = "Froid " + String(T_int, 1) + "C";
        } else if (T_int >= cfg.temp_heater_high) {
            want_heater = false;
            d.heater_reason = "Chaud " + String(T_int, 1) + "C";
        } else {
            want_heater = gpio.heater;   // thermostat: maintient l'etat actuel
            d.heater_reason = "Thermo " + String(T_int, 1) + "C";
        }
        if (d.heater_clim) {
            d.heater_reason = "[CLIM] " + d.heater_reason;
        } else if (manual_active) {
            long remaining = (state.heater_manual_off_at - now) / 60L;
            d.heater_reason = "[" + String(remaining) + "min] " + d.heater_reason;
        }
    } else {
        if (state.heater_manual_off_at > 0 && now >= state.heater_manual_off_at) {
            d.heater_reason = "Timer expire";
            new_heater_off_at = 0;
        } else {
            d.heater_reason = "Pas de demande";
        }
    }
    d.heater = want_heater;

    // ── Mise a jour de l'etat ────────────────────────────────────
    state.H_ext_prev           = H_ext;
    state.has_H_ext_prev       = true;
    state.t_prev               = now;
    state.dehum_on_since       = new_dehum_on_since;
    state.heater_manual_off_at = new_heater_off_at;

    return d;
}
