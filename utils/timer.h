#pragma once
#include <stdbool.h>

void start_or_reset_timer(int seconds);

void reset_timer();

void stop_timer();

bool timer_has_finished_running();
