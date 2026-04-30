#pragma once
#include "hvac_types.h"

void storage_load_config(HvacConfig& cfg);
void storage_save_config(const HvacConfig& cfg);
void storage_load_state(HvacState& state);
void storage_save_state(const HvacState& state);
