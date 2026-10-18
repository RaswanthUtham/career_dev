#include<pthread.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

void *producer(void *arg) {
    int *sample = arg;
    printf("Received from main: %d\n", *sample);
    return NULL;
}

int main() {
    pthread_t worker;
    int sample = 42;
    int error = pthread_create(&worker, NULL, producer, &sample);
    if (error!=0) {
        fprintf(stderr, "pthread create error: %s", strerror(error));
        return EXIT_FAILURE;
    }
    error = pthread_join(worker, NULL);
    if(error != 0) {
        fprintf(stderr, "pthread join error: %s", strerror(error));
        return EXIT_FAILURE;
    }
    printf("Finised Execution");
    return EXIT_SUCCESS;
}
