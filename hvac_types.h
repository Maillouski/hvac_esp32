#pragma once
#include <Arduino.h>

struct HvacConfig {
    float humidity_on         = 53.0f;
    float humidity_hysteresis =  3.0f;
    float humidity_fan_max    = 30.0f;
    float temp_min_dehum      = 15.0f;
    float temp_heater_low     = 17.0f;
    float temp_heater_high    = 22.0f;
    float temp_fan_delta      =  2.0f;
    int   heater_default_min  = 30;
    float k_predictive        =  1.0f;
    float rate_min            = 15.0f;
    int   min_run_dehum_min   = 30;
    float humidity_correction =  1.07f;
};

struct Sensors {
    float H_int = 0.0f, T_int = 0.0f;
    float H_ext = 0.0f, T_ext = 0.0f;
    bool  valid = false;
};

struct GpioState {
    bool fan    = false;
    bool dehum  = false;
    bool heater = false;
};

struct HvacState {
    float  H_ext_prev           = 0.0f;
    bool   has_H_ext_prev       = false;
    time_t t_prev               = 0;
    time_t dehum_on_since       = 0;   // 0 = inactif
    time_t heater_manual_off_at = 0;   // 0 = pas de timer
    int8_t manual_fan           = -1;  // -1=auto  0=force OFF  1=force ON
    int8_t manual_dehum         = -1;
};

struct Decisions {
    bool   fan          = false;
    String fan_reason;
    bool   dehum        = false;
    String dehum_reason;
    bool   heater       = false;
    String heater_reason;
    bool   heater_clim  = false;
    float  rate_ext     = 0.0f;
    float  seuil_on     = 0.0f;
    float  seuil_off    = 0.0f;
};
