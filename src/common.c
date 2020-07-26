#include "common.h"

/* Mutexes */
pthread_mutex_t log_lock;

/* Measurements FIFO queue */
rpa_queue_t* measurements_queue;

/* Config structure */
cfg_opt_t opts[] = {CFG_INT("CONFIG_PRU_NO", 1, CFGF_NONE),
                    CFG_INT("CONFIG_SUBSYSTEM_ADC", 1, CFGF_NONE),
                    CFG_INT("CONFIG_ADC_AVERAGING_STEPS", 1, CFGF_NONE),
                    CFG_INT("CONFIG_ADC_SAMPLE_DELAY", 24, CFGF_NONE),
                    CFG_INT("CONFIG_ADC_OPEN_DELAY", 238, CFGF_NONE),
                    CFG_INT("CONFIG_ADC_TMR", 1000000000, CFGF_NONE),
                    CFG_INT("CONFIG_ADC_ENCODING", 4, CFGF_NONE),
                    CFG_INT("CONFIG_ADC_PIN_MASK", 0b000000010, CFGF_NONE),
                    CFG_INT("CONFIG_ADC_RB_SAMPLES_PER_PORT", 1000, CFGF_NONE),
                    CFG_INT("CONFIG_HOST_ADC_QUEUE_DISCARD", 100, CFGF_NONE),
                    CFG_INT("CONFIG_HOST_ADC_QUEUE_MAX_SIZE", 200, CFGF_NONE),
                    CFG_STR("MQTT_BROKER_ADDRESS", "tcp://192.168.178.16:1883", CFGF_NONE),
                    CFG_STR("MQTT_CLIENTID", "lfd-slave-1", CFGF_NONE),
                    CFG_STR("MQTT_DEFAULT_TOPIC_PREFIX", "lfd/adc", CFGF_NONE),
                    CFG_INT("MQTT_DEFAULT_QOS", 0, CFGF_NONE),
                    CFG_INT("MQTT_KEEP_ALIVE", 30, CFGF_NONE),
                    CFG_INT("MQTT_RECONNECT_TIMER", 5, CFGF_NONE),
                    CFG_END()};
cfg_t* cfg;

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
 * Initialize logger service with mutex
 ********************************************************************************/
int init_logger() {
    /* Set logger level */
    log_set_level(LOG_LEVEL);
    /* Set up mutex for logger service */
    log_set_udata(&log_lock);
    log_set_lock(lock_callback);
    return 0;  // Has no actual error handling
}

/********************************************************************************
 * Initialize configuration
 ********************************************************************************/
int init_config() {
    cfg = cfg_init(opts, CFGF_NONE);
    int e = cfg_parse(cfg, "config.conf");
    if (e == CFG_PARSE_ERROR || e == CFG_FILE_ERROR) {
        return 1;
    }
    return 0;
}