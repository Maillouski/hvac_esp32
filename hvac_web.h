#pragma once
#include "hvac_types.h"

void web_begin(HvacConfig& cfg, HvacState& state,
               Sensors& sensors, GpioState& gpio, Decisions& decisions);
void web_handle();
