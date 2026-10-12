#pragma once
#include <stdbool.h>
#include <stdint.h>

/// @brief Countdown timer. It doesn't use threads: game loop asks it if time is up.
struct Timer
{
    bool is_running;
    int64_t deadline_ms;
};

/// @brief Milliseconds from some fixed moment (monotonic clock, not affected by clock changes).
int64_t now_ms();

/// @brief Starts timer or restarts it if it is already running.
void start_or_reset_timer(struct Timer *timer, int seconds);

/// @brief Stops timer. Stopped timer never finishes.
void stop_timer(struct Timer *timer);

/// @brief Was timer started and is time up.
bool timer_has_finished_running(struct Timer *timer);

/// @brief Seconds left (rounded up). 0 if timer is not running or finished.
int timer_seconds_remaining(struct Timer *timer);
