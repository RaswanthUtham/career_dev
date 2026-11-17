#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

typedef enum {
    REQ_PENDING = 0,
    REQ_CANCELLED,
    REQ_COMPLETED,
    REQ_TIME_OUT
}req_status_t;

typedef struct{
pthread_mutex_t lock;
pthread_cond_t cond;
pthread_t worker_tid;
int payload;
req_status_t status;
bool worker_fineshed;
bool thread_started;
}async_request_t;

void check (int err, char *msg)
{
    if (err != 0) {
        fprintf(stderr, "%s: %s", msg, stderror(err));
        exit(EXIT_FAILURE);
    }
}

static void* hardware_worker(void *arg)
{
    async_request_t *req = (async_request_t *)arg;

    usleep(50000);

    check(pthread_mutex_lock(&req->lock), "worker lock failed");
    if(req->status == REQ_PENDING)
    {
        req->payload = 50;
        req->status = REQ_COMPLETED;
    }
    req->worker_fineshed = true;
    check(pthread_cond_signal(&req->cond), "worker cond failed");
    check(pthread_mutex_unlock(&req->lock), "worker unlock failed");

    return NULL;
}

void async_req_init(async_request_t *req)
{
    pthread_cond_attr_t attr;
    check(pthread_condattr_init(&attr), "asunc req init cond attr init failed");
    check(pthread_condattr_setclock(&attr, CLOCK_MONOTONIC), "cond attr set clock failed");
    check(pthread_cond_init(&req->cond, attr), "cond init failed");
    check(pthread_mutex_init(&req->lock, NULL), "lock init failed");
    check(pthread_condattr_destroy(&attr), "condattr destroy failed");

    req->state = REQ_PENDING;
    req->payload = 0;
    req->thread_started = false;
    req->worker_fineshed = false;
}

void async_request_submit(async_request_t *req)
{
    check(pthread_create(&req->worker_tid, NULL, hardware_worker, req);
    req->thread_started = true;
}

int async_request_wait(async_request_t *req, long timeout_ms)
{
    struct timespec deadline;
    int rc = 0;

    clock_gettime(MONOTONIC, &deadline);
    deadline.tv_sec += timeout_ms / 1000;
    deadline.tv_nsec += (timeout % 1000) * 1000000;
    if(deadline.tv_nsec >= 1000000000L)
    {
        deadline.tv_sec += 1;
        deadline.tv_nsec -= 1000000000L;
    }

    check(pthread_mutex_lock(&req->lock), "wait lock failed");

    while(!req->worker_fineshed && rc != ETIMEDOUT)
    {
        rc = pthread_cond_timedwait(&req->cond, &req->lock, &deadline);
    }

    if(!req->worker_fineshed && rc == ETIMEDOUT)
    {
        req->status = REQ_TIME_OUT;
    }
    int state = req->status;
    check(pthread_mutex_unlock(&req->lock), "wait unlock failed");
    return state;
}

void async_req_cancel(async_request_t *req)
{
    check(pthread_mutex_lock(&req->lock), "cancel lock failed");

    if(req->status == REQ_PENDING)
    {
        req->status = REQ_CANCELLED;
    }

    while(!req->worker_fineshed)
    {
        pthread_cond_wait(&req->cond, &req->lock);
    }

    check(pthread_mutex_unlock(&req->lock), "Cancel unlock failed");
}

void async_req_destroy(async_request_t *req)
{
    if(req->thread_started)
    {
        pthread_join(req->worker_tid, NULL);
        req->thread_started = false
    }
    pthread_cond_destroy(&req->cond);
    pthread_mutex_destroy(&req->mutex);
}

int main()
{
    async_request_t req;

    async_req_init(&req);
    async_req_submit(&req);

    int status = async_request_wait(&req, 100);
    if (status == REQ_COMPLETED) {
        printf("[App] Success! Received payload: %d\n", req.payload);
    } else {
        printf("[App] Failed with status: %d\n", status);
    }
    async_req_destroy(&req);
    printf("[App] Scenario 1 cleaned up cleanly.\n\n");
}
