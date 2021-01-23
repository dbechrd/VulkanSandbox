#include "ta_timer.hpp"
#include "SDL/SDL_timer.h"

static uint64_t perf_frequency;
static double perf_frequency_ms;
static double perf_frequency_us;
static uint64_t perf_epoch;

void ta_timer_init()
{
    // 10,000,000 (per second)
    perf_frequency = SDL_GetPerformanceFrequency();
    // 10,000 (per millisecond)
    perf_frequency_ms = perf_frequency / 1000.0;
    perf_frequency_us = perf_frequency_ms / 1000.0;
    perf_epoch = SDL_GetPerformanceCounter();
}

uint64_t ta_timer_elapsed_ticks()
{
    uint64_t now = SDL_GetPerformanceCounter();
    uint64_t elapsed_ticks = now - perf_epoch;
    return elapsed_ticks;
}

double ta_timer_elapsed_ms()
{
    uint64_t elapsed_ticks = ta_timer_elapsed_ticks();
    double elapsed_ms = elapsed_ticks / perf_frequency_ms;
    return elapsed_ms;
}

double ta_timer_elapsed_us()
{
    uint64_t elapsed_ticks = ta_timer_elapsed_ticks();
    double elapsed_us = elapsed_ticks / perf_frequency_us;
    return elapsed_us;
}

double ta_timer_elapsed_sec()
{
    double elapsed_ms = ta_timer_elapsed_ms();
    double elapsed_sec = elapsed_ms / 1000;
    return elapsed_sec;
}

// Number of milliseconds since last second (modulo)
uint64_t ta_timer_only_ms()
{
    uint64_t elapsed_ticks = ta_timer_elapsed_ticks();
    uint64_t ticks_per_ms = perf_frequency / 1000;
    uint64_t now_ms = elapsed_ticks % ticks_per_ms;
    return now_ms;
}