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

/* Mutexes*/
extern pthread_mutex_t log_lock;
extern pthread_mutex_t measurements_buffer_lock;

/* Global measurements_buffer */
extern AdcReading measurements_buffer[CONFIG_HOST_ADC_BUFFER_SIZE];

/* Init logger service */
void init_logger();

/* Log level */
#define LOG_LEVEL LOG_DEBUG

#define MASK_LOGGING_CODE(level, code) \
  if (!(level < LOG_LEVEL)) {          \
    code                               \
  }

#endif /* COMMON_H */