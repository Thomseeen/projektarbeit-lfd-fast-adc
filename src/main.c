/*
Compile by: `gcc -Wall -o 1 1.c -lpruio`
*/

#include "config.h"
#include "libpruio/pruio.h"
#include "stdint.h"
#include "stdio.h"
#include "unistd.h"

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

  if (pruio_config(io, 10, 0x1FE, CONFIG_ADC_TMR, CONFIG_ADC_ENCODING)) {
    printf("config failed (%s)\n", io->Errr);
  } else {
    pruio_rb_start(io);
    printf("Number of active steps: %i\n", io->Adc->ChAz);
    printf("Number of samples: %i\n", io->Adc->Samples);

    while (1) {
      printf("Stored values cnt: %u\r", (uint32_t)io->DRam[0]);
      usleep(10);
    }

    /*
    for (int ii = 1; ii < 9; ii++) {
      uint16_t adc_reading_raw = io->Adc->Value[ii];
      float adc_reading = (float)adc_reading_raw / (float)0xFFF0 * 1.8f;
      printf("AD%i: %fV\n", ii, adc_reading);
    }
    */
  }

  pruio_destroy(io);
  return 0;
}