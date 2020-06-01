/********************************************************************************
 * adc_reading.h
 ********************************************************************************/
#ifndef ADC_READING_H
#define ADC_READING_H

struct AdcReading_s {
  uint16_t value;       // Actual 16bit ADC-reading
  uint64_t seq_no;      // Measurement sequence number
  uint8_t pin_no;       // Number of the AIN-pin
  uint8_t status_flag;  // Has been sent - 0: unsent new value, 1: sending, 2: sent, 3: initialized
};

typedef struct AdcReading_s AdcReading;

#endif /* ADC_READING_H */