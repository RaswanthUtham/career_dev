#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

pthread_mutex_t lock = PTHREAD_LOCK_INITIALIZER;
pthread_cond_t cv = PTHREAD_COND_INITIALIZER;

struct
{
    int raw;
    int sensor;
}telemetry;

static bool ready;

void check(int error)
{
    if(error != 0)
    {
        fprintf(stderr, "Issue observed: %s\n", strerror(error));
        exit(EXIT_FAILURE);
    }
}

void* producer(void *arg)
{
    (void)arg;
    check(pthread_mutex_lock(&lock));
    telemetry.raw = 10;
    telemetry.sensor = 23;
    ready = 1;
    check(pthread_cond_signal(&cv));
    check(pthread_mutex_unlock(&lock));
    return NULL;
}

int main()
{
    pthread_t worker;
    check(pthread_create(&worker, NULL, producer, NULL));
    check(pthread_mutex_lock(&lock));
    while(!ready)
    {
        check(pthread_cond_wait(&cv, &lock));
    }
    printf("Telemetry Value: %d %d\n", telemetry.raw, telemetry.sensor)
    check(pthread_mutex_unlock(&lock));
    pthread_join(worker, NULL);
    pthread_cond_destroy(&cv);
    pthread_mutex_destroy(&lock);
    printf("Main Succeeded");
    return EXIT_SUCCESS;
}
