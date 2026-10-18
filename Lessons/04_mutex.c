#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

void check(int error)
{
    if(error != 0)
    {
        fprintf(stderr, "Issue Found: %s\n", strerror(error));
        exit(EXIT_FAILURE);
    }
}

static int count;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void *counter(void *arg)
{
    (void)arg;
    for (int i=0; i<100000; i++)
    {
        check(pthread_mutex_lock(&lock));
        count++;
        check(pthread_mutex_unlock(&lock));
    }
    return NULL;
}

int main()
{
    pthread_t worker;
    check(pthread_create(&worker, NULL, counter, NULL));
    counter(NULL);
    check(pthread_join(worker, NULL));
    check(pthread_mutex_destroy(&lock));
    printf("Main Completed with counter value: %d\n", count);
    return EXIT_SUCCESS;
}
