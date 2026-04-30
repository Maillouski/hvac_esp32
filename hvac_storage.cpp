#include "hvac_storage.h"
#include <Preferences.h>

// Espace NVS separe pour config et etat afin d'eviter les collisions
static Preferences prefs;

void storage_load_config(HvacConfig& cfg) {
    prefs.begin("hvac_cfg", /*readOnly=*/true);
    cfg.humidity_on         = prefs.getFloat("hum_on",      53.0f);
    cfg.humidity_hysteresis = prefs.getFloat("hum_hyst",     3.0f);
    cfg.humidity_fan_max    = prefs.getFloat("hum_fan_max", 30.0f);
    cfg.temp_min_dehum      = prefs.getFloat("t_min_dehum", 15.0f);
    cfg.temp_heater_low     = prefs.getFloat("t_heat_low",  17.0f);
    cfg.temp_heater_high    = prefs.getFloat("t_heat_high", 22.0f);
    cfg.temp_fan_delta      = prefs.getFloat("t_fan_delta",  2.0f);
    cfg.heater_default_min  = prefs.getInt  ("heat_def_min", 30);
    cfg.k_predictive        = prefs.getFloat("k_pred",       1.0f);
    cfg.rate_min            = prefs.getFloat("rate_min",    15.0f);
    cfg.min_run_dehum_min   = prefs.getInt  ("min_run_dehum",30);
    cfg.humidity_correction = prefs.getFloat("hum_corr",    1.07f);
    prefs.end();
}

void storage_save_config(const HvacConfig& cfg) {
    prefs.begin("hvac_cfg", /*readOnly=*/false);
    prefs.putFloat("hum_on",      cfg.humidity_on);
    prefs.putFloat("hum_hyst",    cfg.humidity_hysteresis);
    prefs.putFloat("hum_fan_max", cfg.humidity_fan_max);
    prefs.putFloat("t_min_dehum", cfg.temp_min_dehum);
    prefs.putFloat("t_heat_low",  cfg.temp_heater_low);
    prefs.putFloat("t_heat_high", cfg.temp_heater_high);
    prefs.putFloat("t_fan_delta", cfg.temp_fan_delta);
    prefs.putInt  ("heat_def_min",cfg.heater_default_min);
    prefs.putFloat("k_pred",      cfg.k_predictive);
    prefs.putFloat("rate_min",    cfg.rate_min);
    prefs.putInt  ("min_run_dehum",cfg.min_run_dehum_min);
    prefs.putFloat("hum_corr",    cfg.humidity_correction);
    prefs.end();
}

void storage_load_state(HvacState& state) {
    prefs.begin("hvac_state", /*readOnly=*/true);
    state.has_H_ext_prev       = prefs.getBool  ("has_h_ext",   false);
    state.H_ext_prev           = prefs.getFloat ("h_ext_prev",  0.0f);
    state.t_prev               = (time_t)prefs.getLong64("t_prev",      0LL);
    state.dehum_on_since       = (time_t)prefs.getLong64("dehum_since", 0LL);
    state.heater_manual_off_at = (time_t)prefs.getLong64("heat_off_at", 0LL);
    state.manual_fan           = (int8_t)prefs.getChar("manual_fan",   -1);
    state.manual_dehum         = (int8_t)prefs.getChar("manual_dehum", -1);
    prefs.end();
}

void storage_save_state(const HvacState& state) {
    prefs.begin("hvac_state", /*readOnly=*/false);
    prefs.putBool  ("has_h_ext",   state.has_H_ext_prev);
    prefs.putFloat ("h_ext_prev",  state.H_ext_prev);
    prefs.putLong64("t_prev",      (int64_t)state.t_prev);
    prefs.putLong64("dehum_since", (int64_t)state.dehum_on_since);
    prefs.putLong64("heat_off_at", (int64_t)state.heater_manual_off_at);
    prefs.putChar  ("manual_fan",  state.manual_fan);
    prefs.putChar  ("manual_dehum",state.manual_dehum);
    prefs.end();
}
