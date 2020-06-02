#include "common.h"

/* Mutexes */
pthread_mutex_t log_lock;
pthread_mutex_t measurements_buffer_lock;

/* Measurements buffer */
volatile AdcReading measurements_buffer[CONFIG_HOST_ADC_BUFFER_SIZE];

/********************************************************************************
 * Callback for logger service to obtain lock
 ********************************************************************************/
static void lock_callback(void* udata, int lock) {
    if (lock) {
        pthread_mutex_lock(&log_lock);
    } else {
        pthread_mutex_unlock(&log_lock);
    }
}

/********************************************************************************
 * Initializr logger service with mutex
 ********************************************************************************/
void init_logger() {
    /* Set logger level */
    log_set_level(LOG_LEVEL);
    /* Set up mutex for logger service */
    log_set_udata(&log_lock);
    log_set_lock(lock_callback);
}