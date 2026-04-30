#pragma once
#include "hvac_types.h"

Decisions compute(const Sensors& sensors, const HvacConfig& cfg,
                  HvacState& state, const GpioState& gpio);
