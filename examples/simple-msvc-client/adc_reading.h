/********************************************************************************
 * adc_reading.h
 ********************************************************************************/
#ifndef ADC_READING_H
#define ADC_READING_H

typedef enum {
    ADC_READ_NEW_VALUE,
    ADC_READ_GRABBED_FROM_QUEUE,
    ADC_READ_SENDING,
    ADC_READ_SENT
} AdcReadingStatus;

struct AdcReading_s {
    uint16_t value;   // Actual 16bit ADC-reading
    uint64_t seq_no;  // Measurement sequence number
    uint8_t pin_no;   // Number of the AIN-pin
    AdcReadingStatus status;
};

typedef struct AdcReading_s AdcReading;

#endif /* ADC_READING_H */