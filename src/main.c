#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "adc_reading.h"
#include "config.h"
#include "libpruio/pruio.h"

/********************************************************************************
 * MAIN
 ********************************************************************************/
int main(int argc, char** argv) {
  /* Build configuration for pruio */
  uint16_t config_pruio_subsystems;
  config_pruio_subsystems = CONFIG_PRU_NO ? PRUIO_ACT_PRU1 : 0;
  config_pruio_subsystems |= CONFIG_SUBSYSTEM_ADC ? PRUIO_ACT_ADC : PRUIO_ACT_ADC;

  pruIo* io = pruio_new(config_pruio_subsystems, CONFIG_ADC_AVERAGING_STEPS, CONFIG_ADC_OPEN_DELAY,
                        CONFIG_ADC_SAMPLE_DELAY);

  /* Setup PRUIO driver */
  if (pruio_config(io, CONFIG_ADC_RB_SAMPLES_PER_PORT, CONFIG_ADC_PIN_MASK, CONFIG_ADC_TMR,
                   CONFIG_ADC_ENCODING)) {
    fprintf(stderr, "FATAL: PRUIO config failed (%s). Exiting...\n", io->Errr);
    exit(-1);
  }

  /* Build measurements buffer */
  AdcReading measurements_buffer[CONFIG_HOST_ADC_BUFFER_SIZE];

  /* Start rb-mode */
  if (pruio_rb_start(io)) {
    fprintf(stderr, "FATAL: PRUIO rb mode start failed (%s). Exiting...\n", io->Errr);
    exit(-1);
  }

  uint32_t adc_rb_samples_wraparound = io->Adc->Samples;
  uint8_t adc_active_pin_cnt = io->Adc->ChAz;

  printf("INFO: Number of active steps: %i\n", adc_rb_samples_wraparound);
  printf("INFO: Number of samples: %i\n", adc_active_pin_cnt);

  /* Init counters */
  int32_t sample_cnt_last = -1;
  uint32_t host_adc_buffer_indexer = 0;
  uint64_t seq_numbers[adc_active_pin_cnt];
  for (int ii = 0; ii < adc_active_pin_cnt; ii++) {
    seq_numbers[ii] = 0;
  }

  /* Main loop */
  while (1) {
    /* Wait till rb-mode starts */
    if (io->DRam[0] == PRUIO_MSG_MM_WAIT || io->DRam[0] == PRUIO_MSG_MM_TRG1) {
      printf("INFO: Waiting for RB-mode to start\n");
      while (io->DRam[0] == PRUIO_MSG_MM_WAIT || io->DRam[0] == PRUIO_MSG_MM_TRG1) {
        ;
      }
      printf("INFO: RB-mode started\n");
    }
    /* Actual sampling */
    else {
      int32_t sample_cnt_current = io->DRam[0];
      if (sample_cnt_current != sample_cnt_last) {
        printf("DEBUG: ----- New Values -----\n");

        int32_t start_index, end_index;
        /* Wrap-around handling rb - no wrap-around yet */
        if (sample_cnt_current > sample_cnt_last) {
          start_index = sample_cnt_last;
          end_index = sample_cnt_current;
        }
        /* Wrap-around handling rb - wrap-around occured, previously hit last buffer entry */
        else if (sample_cnt_last == (adc_rb_samples_wraparound - 1)) {
          start_index = -1;
          end_index = sample_cnt_current;
        }
        /* Wrap-around handling rb - wrap-around occured*/
        else {
          start_index = sample_cnt_last;
          end_index = (adc_rb_samples_wraparound - 1);
        }

        if (sample_cnt_current < sample_cnt_last)
          printf("DEBUG: Wrap-around occured\n");

        printf("DEBUG: sample_cnt_last %i, sample_cnt_current %i, start_index %i, end_index %i\n",
               sample_cnt_last, sample_cnt_current, start_index, end_index);

        /* Write all new samples into measurements buffer */
        for (int ii = start_index + 1; ii <= end_index; ii++) {
          uint8_t pin_no = ii % adc_active_pin_cnt;

          /*
          if (!measurements_buffer[host_adc_buffer_indexer].sent_flag) {
            printf("WARNING: Dropped measurement pin %i at seq_no %i",
                   measurements_buffer[host_adc_buffer_indexer].pin_no,
                   measurements_buffer[host_adc_buffer_indexer].seq_no);
          }
          */

          printf("DEBUG: Writing to host buffer at idx %i\n", host_adc_buffer_indexer);

          measurements_buffer[host_adc_buffer_indexer].pin_no = pin_no;
          measurements_buffer[host_adc_buffer_indexer].value = io->Adc->Value[ii];
          measurements_buffer[host_adc_buffer_indexer].seq_no = seq_numbers[pin_no];
          measurements_buffer[host_adc_buffer_indexer].sent_flag = 0;

          /* Handling wrap-arounds */
          /* Wrap-round handling seq_no */
          if (seq_numbers[pin_no] >= (0xFFFFFFFFFFFFFFFF - 1)) {
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
      }
    }
  }

  pruio_destroy(io);

  exit(0);
}