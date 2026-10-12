#include "timer.h"
#include <time.h> // clock_gettime()

int64_t now_ms()
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

void start_or_reset_timer(struct Timer *timer, int seconds)
{
    timer->is_running = true;
    timer->deadline_ms = now_ms() + (int64_t)seconds * 1000;
}

void stop_timer(struct Timer *timer)
{
    timer->is_running = false;
}

bool timer_has_finished_running(struct Timer *timer)
{
    return timer->is_running && now_ms() >= timer->deadline_ms;
}

int timer_seconds_remaining(struct Timer *timer)
{
    if (!timer->is_running)
    {
        return 0;
    }
    int64_t left = timer->deadline_ms - now_ms();
    if (left <= 0)
    {
        return 0;
    }
    return (int)((left + 999) / 1000);
}
