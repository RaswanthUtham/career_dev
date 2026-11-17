#include <pthread.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static int measurement;
atomic_bool ready = false;

void* producer(void *arg)
{
    (void)arg;
    measurement = 45;
    atomic_store_explicit(&ready, true, memory_order_release);
    return NULL;
}

int main()
{
    pthread_t worker;
    int error = pthread_create(&worker, NULL, producer, NULL);
    if (error != 0)
    {
        fprintf(stderr, "thread creation failed: %s \n", strerror(error));
        return EXIT_FAILURE;
    }

    while(atomic_load_explicit(&ready, memory_order_acquire) == 0);
    printf("Reveived Measurement is: %d\n", measurement);

    error = pthread_join(worker, NULL);
    if (error != 0)
    {
        fprintf(stderr, "thread join failed: %s \n", strerror(error));
        return EXIT_FAILURE;
    }
    printf("Main Finished");
    return EXIT_SUCCESS;
}
