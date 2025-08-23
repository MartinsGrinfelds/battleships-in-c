#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>

static pthread_t timer_thread;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static bool is_running = false;
static bool should_stop = false;
static bool should_reset = false;
static bool has_finished_running = false;
static int duration_seconds = 0;

static void* timer_func() {
    int remaining_seconds = duration_seconds;
    has_finished_running = false;

    while (remaining_seconds > 0 && !should_stop) {
        sleep(1);

        pthread_mutex_lock(&lock);
        if (should_reset) {
            remaining_seconds = duration_seconds;
            should_reset = false;
            has_finished_running = false;
            printf("timer reset!\n");
        } else {
            remaining_seconds--;
            printf("time remaining: %ds\n", remaining_seconds);
        }
        pthread_mutex_unlock(&lock);
    }

    if (remaining_seconds == 0 && !should_stop) {
        has_finished_running = true;
        printf("timer expired!\n");
    }

    is_running = false;
    return NULL;
}

void start_or_reset_timer(int seconds) {
    pthread_mutex_lock(&lock);
    duration_seconds = seconds;
    if (is_running) {
        should_reset = true;
        pthread_mutex_unlock(&lock);
        return;
    }
    pthread_mutex_unlock(&lock);
    pthread_create(&timer_thread, NULL, timer_func, NULL);
}

void stop_timer() {
    if (!is_running)
    {
        return;
    }
    should_stop = true;
    pthread_join(timer_thread, NULL);
    printf("timer stopped!\n");
}

bool timer_has_finished_running() {
    return has_finished_running;
}
