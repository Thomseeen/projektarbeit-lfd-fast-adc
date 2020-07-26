/********************************************************************************
 * common.h
 ********************************************************************************/
#ifndef COMMON_H
#define COMMON_H

/* Global includes */
#include <MQTTAsync.h>
#include <confuse.h>
#include <inttypes.h>  // fixed size types
#include <pthread.h>   // threads
#include <stdlib.h>    // error handling
#include <unistd.h>    // sleep/usleep

#include "adc_reading.h"
#include "config_base.h"
#include "log.h"
#include "rpa_queue.h"

/* Mutexes, definition in common.c, init in main.c */
extern pthread_mutex_t log_lock;

/* Global measurements_queue, definition in common.c */
extern rpa_queue_t* measurements_queue;

/* Pointer to config structure */
extern cfg_t* cfg;

/* Init helpers */
int init_logger();
int init_config();

#endif /* COMMON_H */