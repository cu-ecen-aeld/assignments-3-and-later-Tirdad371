#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;

    if(thread_func_args == NULL)
    {
        return NULL;
    }

    // Default to false until successful completion
    thread_func_args->thread_complete_success = false;

    // 1. Wait before attempting to obtain mutex
    // usleep takes microseconds, so multiply ms by 1000
    usleep(thread_func_args->wait_to_obtain_ms * 1000);

    // 2. Obtain mutex
    if(pthread_mutex_lock(thread_func_args->mutex) != 0)
    {
        ERROR_LOG("Failed to lock mutex");
        return thread_param; 
    }

    // 3. Hold mutex for specified time
    usleep(thread_func_args->wait_to_release_ms * 1000);

    // 4. Release mutex
    if(pthread_mutex_unlock(thread_func_args->mutex) != 0)
    {
        ERROR_LOG("Failed to unlock mutex");
        return thread_param;
    }

    // Success!
    thread_func_args->thread_complete_success = true;

    return thread_param;
}

bool start_thread_obtaining_mutex(pthread_t *thread,
                                  pthread_mutex_t *mutex,
                                  int wait_to_obtain_ms,
                                  int wait_to_release_ms)
{
    if(thread == NULL || mutex == NULL)
    {
        return false;
    }

    // Allocate thread data dynamically as required by threading.h
    struct thread_data *data = malloc(sizeof(struct thread_data));
    if(data == NULL)
    {
        ERROR_LOG("Failed to allocate thread_data");
        return false;
    }

    // Initialize the structure
    data->mutex = mutex;
    data->wait_to_obtain_ms = wait_to_obtain_ms;
    data->wait_to_release_ms = wait_to_release_ms;
    data->thread_complete_success = false;

    // Create the thread
    int rc = pthread_create(thread, NULL, threadfunc, data);
    if(rc != 0)
    {
        ERROR_LOG("Failed to create thread with code %d", rc);
        free(data); // Clean up if thread creation fails
        return false;
    }

    return true;
}
