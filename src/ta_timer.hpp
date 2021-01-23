#pragma once
#include <cstdint>

void ta_timer_init              ();
uint64_t ta_timer_elapsed_ticks ();
double ta_timer_elapsed_ms      ();
double ta_timer_elapsed_sec     ();
uint64_t ta_timer_only_ms       ();