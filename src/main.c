#include <libpruio/pruio.h>
#include <stdlib.h>

#include "common.h"
#include "mqtt_handler.h"

/********************************************************************************
 * Custom functions
 ********************************************************************************/
void cleanup(pruIo* io, pthread_mutex_t* log_lock, pthread_mutex_t* measurements_buffer_lock) {
    log_info("Destroying PRUIO driver structure");
    pthread_mutex_destroy(log_lock);
    pthread_mutex_destroy(measurements_buffer_lock);
    if (io) {
        pruio_destroy(io);
    }
}

/********************************************************************************
 * MAIN
 ********************************************************************************/
int main(int argc, char** argv) {
    /* Initialize Mutexes */
    if (pthread_mutex_init(&log_lock, NULL) != 0) {
        log_fatal("Log mutex could not be initialized");
        cleanup(NULL, &log_lock, &measurements_buffer_lock);
        exit(EXIT_FAILURE);
    }
    if (pthread_mutex_init(&measurements_buffer_lock, NULL) != 0) {
        log_fatal("Measurements buffer mutex could not be initialized");
        cleanup(NULL, &log_lock, &measurements_buffer_lock);
        exit(EXIT_FAILURE);
    }

    init_logger();

    /* Build configuration for pruio */
    uint16_t config_pruio_subsystems;
    config_pruio_subsystems = CONFIG_PRU_NO ? PRUIO_ACT_PRU1 : 0;
    config_pruio_subsystems |= CONFIG_SUBSYSTEM_ADC ? PRUIO_ACT_ADC : PRUIO_ACT_ADC;

    pruIo* io = pruio_new(config_pruio_subsystems, CONFIG_ADC_AVERAGING_STEPS,
                          CONFIG_ADC_OPEN_DELAY, CONFIG_ADC_SAMPLE_DELAY);

    /* Setup PRUIO driver */
    if (pruio_config(io, CONFIG_ADC_RB_SAMPLES_PER_PORT, CONFIG_ADC_PIN_MASK, CONFIG_ADC_TMR,
                     CONFIG_ADC_ENCODING)) {
        log_fatal("PRUIO config failed (%s). Exiting...", io->Errr);
        cleanup(io, &log_lock, &measurements_buffer_lock);
        exit(EXIT_FAILURE);
    }

    /* Set all status flags to initialized */
    const AdcReading AdcReading_default = {0, 0, 0, 0, ADC_READ_INITIALIZED};
    pthread_mutex_lock(&measurements_buffer_lock);
    for (uint32_t ii = 0; ii < CONFIG_HOST_ADC_BUFFER_SIZE; ii++) {
        measurements_buffer[ii] = AdcReading_default;
    }
    pthread_mutex_unlock(&measurements_buffer_lock);

    /* Start MQTT-Handler thread */
    pthread_t mqtt_handler_thread_id;
    int rc = pthread_create(&mqtt_handler_thread_id, NULL, mqtt_handler, NULL);
    if (rc) {
        log_fatal("Unable to create MQTT-Handler thread, %d", rc);
        cleanup(io, &log_lock, &measurements_buffer_lock);
        exit(EXIT_FAILURE);
    }

    /* Start rb-mode */
    if (pruio_rb_start(io)) {
        log_fatal("PRUIO rb mode start failed (%s). Exiting...", io->Errr);
        cleanup(io, &log_lock, &measurements_buffer_lock);
        exit(EXIT_FAILURE);
    }

    uint32_t adc_rb_samples_wraparound = io->Adc->Samples;
    uint8_t adc_active_pin_cnt = io->Adc->ChAz;

    log_info("Number of active steps: %lu", adc_rb_samples_wraparound);
    log_info("Number of samples: %hhu", adc_active_pin_cnt);

    /* Calculate actual active AIN numbers */
    uint8_t adc_active_pins[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    for (uint8_t ii = 1, jj = 0; ii <= LFD_MAX_ADC_PINS; ii++) {
        if ((CONFIG_ADC_PIN_MASK >> ii) & 0xFF1) {
            adc_active_pins[jj] = ii - 1;
            jj++;
        }
    }
    for (uint8_t ii = 0; ii < adc_active_pin_cnt; ii++) {
        log_info("Active pin %hhu: AIN%hhu", ii, adc_active_pins[ii]);
    }

    /* Init counters */
    int32_t sample_cnt_last = -1;
    uint32_t host_adc_buffer_indexer = 0;
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
                /* Wrap-around handling rb - wrap-around occured, previously hit last buffer entry
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

                // clang-format off
                MASK_LOGGING_CODE(LOG_TRACE,
                    if (sample_cnt_current < sample_cnt_last) log_trace("Wrap-around occured in rb");
                    log_trace("sample_cnt_last %li, sample_cnt_current %li, start_index %li, end_index %li",
                            sample_cnt_last, sample_cnt_current, start_index, end_index);
                )
                // clang-format on

                /* Write all new samples into measurements buffer */
                for (int32_t ii = start_index + 1; ii <= end_index; ii++) {
                    uint8_t pin_no = ii % adc_active_pin_cnt;

                    /* Check whether measurements have been dropped */
                    pthread_mutex_lock(&measurements_buffer_lock);
                    if (measurements_buffer[host_adc_buffer_indexer].status != ADC_READ_SENT &&
                        measurements_buffer[host_adc_buffer_indexer].status !=
                            ADC_READ_INITIALIZED) {
                        log_warn("Dropped measurement pin %hhu at seq_no %llu with status %hu",
                                 measurements_buffer[host_adc_buffer_indexer].pin_no,
                                 measurements_buffer[host_adc_buffer_indexer].seq_no,
                                 measurements_buffer[host_adc_buffer_indexer].status);
                    }

                    // clang-format off
                    MASK_LOGGING_CODE(LOG_TRACE,
                        log_trace("Writing to host buffer at idx %lu", host_adc_buffer_indexer);
                    )
                    // clang-format on

                    measurements_buffer[host_adc_buffer_indexer].pin_no = adc_active_pins[pin_no];
                    measurements_buffer[host_adc_buffer_indexer].value = io->Adc->Value[ii];
                    measurements_buffer[host_adc_buffer_indexer].seq_no = seq_numbers[pin_no];
                    measurements_buffer[host_adc_buffer_indexer].status = ADC_READ_NEW_VALUE;
                    pthread_mutex_unlock(&measurements_buffer_lock);

                    // clang-format off
                    MASK_LOGGING_CODE(LOG_TRACE,
                        log_trace("Pin %hhu is currently at seq_no %llu", adc_active_pins[pin_no], seq_numbers[pin_no]);
                    )
                    // clang-format on

                    /* Handling wrap-arounds */
                    /* Wrap-round handling seq_no */
                    if (seq_numbers[pin_no] < (0xFFFFFFFFFFFFFFFF - 1)) {
                        seq_numbers[pin_no]++;
                    } else {
                        seq_numbers[pin_no] = 0;
                    }
                    /* Wrap-around handling host buffer */
                    if (host_adc_buffer_indexer < (CONFIG_HOST_ADC_BUFFER_SIZE - 1)) {
                        host_adc_buffer_indexer++;
                    } else {
                        host_adc_buffer_indexer = 0;
                    }
                }
                /* Wrap-around handling rb - if wrap-around, start next round reading from 0 */
                sample_cnt_last = end_index;

                /* Count sample to be sent */
                // clang-format off
                MASK_LOGGING_CODE(LOG_DEBUG,
                    uint32_t unsent_measurement_cnt = 0;
                    pthread_mutex_lock(&measurements_buffer_lock);
                    for (uint32_t ii = 0; ii < CONFIG_HOST_ADC_BUFFER_SIZE; ii++) {
                        unsent_measurement_cnt +=
                            (measurements_buffer[ii].status == ADC_READ_NEW_VALUE) ? 1 : 0;
                    } pthread_mutex_unlock(&measurements_buffer_lock);
                    if (unsent_measurement_cnt) log_debug("%lu unsent measurements in host buffer",
                                                          unsent_measurement_cnt);
                )
                // clang-format on
            }
        }
    }

    /* Destroy div. elements */
    cleanup(io, &log_lock, &measurements_buffer_lock);
    exit(EXIT_SUCCESS);
}