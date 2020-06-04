/********************************************************************************
 * common.h
 ********************************************************************************/
#ifndef COMMON_H
#define COMMON_H

/* Global includes */
#include <MQTTAsync.h>
#include <inttypes.h>
#include <pthread.h>

#include "adc_reading.h"
#include "config.h"
#include "log.h"

/* Mutexes, definition in common.c, init in main.c */
extern pthread_mutex_t log_lock;
extern pthread_mutex_t measurements_buffer_lock[CONFIG_HOST_ADC_BUFFER_SIZE];

/* Global measurements_buffer, definition in common.c */
extern AdcReading measurements_buffer[CONFIG_HOST_ADC_BUFFER_SIZE];

/* Init logger service */
void init_logger();

#endif /* COMMON_H */