/********************************************************************************
 * common.h
 ********************************************************************************/
#ifndef COMMON_H
#define COMMON_H

/* Global includes */
#include <MQTTAsync.h>
#include <inttypes.h>  // fixed size types
#include <pthread.h>   // threads
#include <stdlib.h>    // error handling
#include <unistd.h>    // sleep/usleep

#include "adc_reading.h"
#include "config.h"
#include "log.h"
#include "rpa_queue.h"

/* Mutexes, definition in common.c, init in main.c */
extern pthread_mutex_t log_lock;

/* Global measurements_queue, definition in common.c */
extern rpa_queue_t* measurements_queue;

/* Init logger service */
void init_logger();

#endif /* COMMON_H */