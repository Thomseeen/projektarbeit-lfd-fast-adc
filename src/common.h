/********************************************************************************
 * common.h
 ********************************************************************************/
#ifndef COMMON_H
#define COMMON_H

/* Global includes */
#include <MQTTClient.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#include "adc_reading.h"
#include "config.h"
#include "log.h"

/* Mutexes, definition in common.c, init in main.c */
extern pthread_mutex_t log_lock;
extern pthread_mutex_t measurements_buffer_lock;

/* Global measurements_buffer, definition in common.c */
extern AdcReading measurements_buffer[CONFIG_HOST_ADC_BUFFER_SIZE];

/* Init logger service */
void init_logger();

#define MASK_LOGGING_CODE(level, code) \
    if (!(level < LOG_LEVEL)) {        \
        code                           \
    }

#endif /* COMMON_H */