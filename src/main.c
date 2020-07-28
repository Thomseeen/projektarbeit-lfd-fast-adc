#include <libpruio/pruio.h>

#include "common.h"
#include "mqtt_handler.h"

/********************************************************************************
 * Custom functions
 ********************************************************************************/
void cleanup(pruIo* io, pthread_mutex_t* log_lock, rpa_queue_t* queue, cfg_t* cfg) {
    log_info("Destroying all the things");
    pthread_mutex_destroy(log_lock);
    if (io) {
        pruio_destroy(io);
    }
    if (queue) {
        rpa_queue_term(queue);
        rpa_queue_destroy(queue);
    }
    cfg_free(cfg);
}

/********************************************************************************
 * MAIN
 ********************************************************************************/
int main(int argc, char** argv) {
    /* Parse arguments */
    char* config_filename = NULL;
    int opt;
    bool stop = false;
    while ((opt = getopt(argc, argv, "c:hv")) != -1) {
        switch (opt) {
            case 'c':
                config_filename = optarg;
                break;
            case 'v':
                printf("lfd-fast-adc version: %s\n", VERSION);
                stop = true;
                break;
            case 'h':
                printf("lfd-fast-adc\n");
                printf("Fast sampling of the BBB's ADC via PRU\n");
                printf("Sampled measurements are published via MQTT\n\n");
                printf("usage: projektarbeit-lfd-fast-adc [options]\n");
                printf("options:\n");
                printf("    -h\t\t\t Show this screen.\n");
                printf("    -v\t\t\t Show program version\n");
                printf("    -c <config_file>\t Specify a config file path.\n");
                stop = true;
                break;
        }
    }

    if (!stop) {
        /* Initialize config structure */
        if (init_config(config_filename)) {
            log_fatal("Configuration could not be initialized");
            cleanup(NULL, &log_lock, measurements_queue, cfg);
            exit(EXIT_FAILURE);
        }

        /* Initialize mutex for logger */
        if (pthread_mutex_init(&log_lock, NULL) != 0) {
            log_fatal("Logger mutex could not be initialized");
            cleanup(NULL, &log_lock, measurements_queue, cfg);
            exit(EXIT_FAILURE);
        }

        /* Initialize logger */
        if (init_logger()) {
            log_fatal("Logger could not be initialized");
            cleanup(NULL, &log_lock, measurements_queue, cfg);
            exit(EXIT_FAILURE);
        }

        /* Build configuration for pruio */
        uint16_t config_pruio_subsystems;
        config_pruio_subsystems = cfg_getint(cfg, "CONFIG_PRU_NO") ? PRUIO_ACT_PRU1 : 0;
        config_pruio_subsystems |=
            cfg_getint(cfg, "CONFIG_SUBSYSTEM_ADC") ? PRUIO_ACT_ADC : PRUIO_ACT_ADC;

        pruIo* io = pruio_new(
            config_pruio_subsystems, cfg_getint(cfg, "CONFIG_ADC_AVERAGING_STEPS"),
            cfg_getint(cfg, "CONFIG_ADC_OPEN_DELAY"), cfg_getint(cfg, "CONFIG_ADC_SAMPLE_DELAY"));

        /* Setup PRUIO driver */
        if (pruio_config(io, cfg_getint(cfg, "CONFIG_ADC_RB_SAMPLES_PER_PORT"),
                         cfg_getint(cfg, "CONFIG_ADC_PIN_MASK"), cfg_getint(cfg, "CONFIG_ADC_TMR"),
                         cfg_getint(cfg, "CONFIG_ADC_ENCODING"))) {
            log_fatal("PRUIO config failed (%s). Exiting...", io->Errr);
            cleanup(io, &log_lock, measurements_queue, cfg);
            exit(EXIT_FAILURE);
        }

        /* Start MQTT-Handler thread */
        pthread_t mqtt_handler_thread_id;
        int rc = pthread_create(&mqtt_handler_thread_id, NULL, mqtt_handler, NULL);
        if (rc) {
            log_fatal("Unable to create MQTT-Handler thread, %d. Exiting...", rc);
            cleanup(io, &log_lock, measurements_queue, cfg);
            exit(EXIT_FAILURE);
        }

        /* Start rb-mode */
        if (pruio_rb_start(io)) {
            log_fatal("PRUIO rb mode start failed (%s). Exiting...", io->Errr);
            cleanup(io, &log_lock, measurements_queue, cfg);
            exit(EXIT_FAILURE);
        }

        /* Setup measurements_queue */
        if (!rpa_queue_create(&measurements_queue,
                              cfg_getint(cfg, "CONFIG_HOST_ADC_QUEUE_MAX_SIZE"))) {
            log_fatal("Measurements queue could not be initialized. Exiting...");
            cleanup(io, &log_lock, measurements_queue, cfg);
            exit(EXIT_FAILURE);
        }

        uint32_t adc_rb_samples_wraparound = io->Adc->Samples;
        uint8_t adc_active_pin_cnt = io->Adc->ChAz;

        log_info("Number of active steps: %lu", adc_rb_samples_wraparound);
        log_info("Number of samples: %hhu", adc_active_pin_cnt);

        /* Calculate actual active AIN numbers */
        uint8_t adc_active_pins[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        for (uint8_t ii = 1, jj = 0; ii <= LFD_MAX_ADC_PINS; ii++) {
            if ((cfg_getint(cfg, "CONFIG_ADC_PIN_MASK") >> ii) & 0xFF1) {
                adc_active_pins[jj] = ii - 1;
                jj++;
            }
        }
        for (uint8_t ii = 0; ii < adc_active_pin_cnt; ii++) {
            log_info("Active pin %hhu: AIN%hhu", ii, adc_active_pins[ii]);
        }

        /* Init counters */
        int32_t sample_cnt_last = -1;
        uint64_t seq_numbers[adc_active_pin_cnt];
        for (uint8_t ii = 0; ii < adc_active_pin_cnt; ii++) {
            seq_numbers[ii] = 0;
            log_trace("Pin %hhi is currently at seq_no %llu", adc_active_pins[ii], seq_numbers[ii]);
        }

        /* Main loop */
        while (1) {
            /* Wait till rb-mode starts */
            if (io->DRam[0] == PRUIO_MSG_MM_WAIT || io->DRam[0] == PRUIO_MSG_MM_TRG1) {
                log_info("Waiting for RB-mode to start");
                while (io->DRam[0] == PRUIO_MSG_MM_WAIT || io->DRam[0] == PRUIO_MSG_MM_TRG1) {
                    ;
                }
                log_info("RB-mode started");
            }
            /* Actual sampling */
            else {
                int32_t sample_cnt_current = io->DRam[0];
                if (sample_cnt_current != sample_cnt_last) {
                    log_trace("----- New Values -----");

                    int32_t start_index, end_index;
                    /* Wrap-around handling rb - no wrap-around yet */
                    if (sample_cnt_current > sample_cnt_last) {
                        start_index = sample_cnt_last;
                        end_index = sample_cnt_current;
                    }
                    /* Wrap-around handling rb - wrap-around occured, previously hit last buffer
                     * entry
                     */
                    else if (sample_cnt_last == (adc_rb_samples_wraparound - 1)) {
                        start_index = -1;
                        end_index = sample_cnt_current;
                    }
                    /* Wrap-around handling rb - wrap-around occured*/
                    else {
                        start_index = sample_cnt_last;
                        end_index = (adc_rb_samples_wraparound - 1);
                    }

#if L_TRACE
                    if (sample_cnt_current < sample_cnt_last) {
                        log_trace("Wrap-around occured in rb");
                    }
                    log_trace(
                        "sample_cnt_last %li, sample_cnt_current %li, start_index %li, end_index "
                        "%li",
                        sample_cnt_last, sample_cnt_current, start_index, end_index);
#endif

                    /* Write all new samples into measurements buffer */
                    for (int32_t ii = start_index + 1; ii <= end_index; ii++) {
                        uint8_t pin_no = ii % adc_active_pin_cnt;

                        /* Grab new memory, popuate with new measurement, add to queue */
                        AdcReading* new_reading = (AdcReading*)malloc(sizeof(AdcReading));
                        new_reading->pin_no = adc_active_pins[pin_no];
                        new_reading->value = io->Adc->Value[ii];
                        new_reading->seq_no = seq_numbers[pin_no];
                        new_reading->status = ADC_READ_NEW_VALUE;

                        if (!rpa_queue_push(measurements_queue, new_reading)) {
                            log_error("Error on pushing new measurement to queue, discard");
                            free(new_reading);
                        }

#if L_TRACE
                        log_trace("Pin %hhu is currently at seq_no %llu", adc_active_pins[pin_no],
                                  seq_numbers[pin_no]);
#endif

                        /* Handling wrap-arounds */
                        /* Wrap-round handling seq_no */
                        if (seq_numbers[pin_no] < (0xFFFFFFFFFFFFFFFF - 1)) {
                            seq_numbers[pin_no]++;
                        } else {
                            seq_numbers[pin_no] = 0;
                        }
                    }
                    /* Wrap-around handling rb - if wrap-around, start next round reading from 0 */
                    sample_cnt_last = end_index;

                    /* Count sample to be sent */
                    uint32_t unsent_measurement_cnt = rpa_queue_size(measurements_queue);
#if L_DEBUG
                    if (unsent_measurement_cnt) {
                        log_debug("%lu unsent measurements in host queue", unsent_measurement_cnt);
                    }
#endif
                    /* Discard measurements of too many have been pilled up */
                    if (unsent_measurement_cnt >=
                        cfg_getint(cfg, "CONFIG_HOST_ADC_QUEUE_DISCARD")) {
                        log_warn("Over %i unsent measurements, discarding",
                                 cfg_getint(cfg, "CONFIG_HOST_ADC_QUEUE_DISCARD"));
                        AdcReading* reading_to_discard = NULL;
                        for (int ii = 0; ii < cfg_getint(cfg, "CONFIG_HOST_ADC_QUEUE_DISCARD");
                             ii++) {
                            rpa_queue_pop(measurements_queue, (void**)&reading_to_discard);
                            free(reading_to_discard);
                        }
                    }
                }
            }
        }
        /* Destroy div. elements */
        cleanup(io, &log_lock, measurements_queue, cfg);
    }
    exit(EXIT_SUCCESS);
}