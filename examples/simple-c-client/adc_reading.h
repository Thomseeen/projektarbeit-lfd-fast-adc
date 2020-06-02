/********************************************************************************
 * adc_reading.h
 ********************************************************************************/
#ifndef ADC_READING_H
#define ADC_READING_H

typedef enum {
    ADC_READ_INITIALIZED,
    ADC_READ_NEW_VALUE,
    ADC_READ_GRABBED_FROM_RB,
    ADC_READ_SENDING,
    ADC_READ_SENT
} AdcReadingStatus;

struct AdcReading_s {
    uint16_t value;              // Actual 16bit ADC-reading        - 2b - 8b
    uint64_t seq_no;             // Measurement sequence number     - 8b - 8b
    uint8_t pin_no;              // Number of the AIN-pin           - 1b - 4b
    MQTTAsync_token mqtt_token;  // To keep track of async sending  - 4b - 4b
    AdcReadingStatus status;     //                                 - 4b - 8b
};

typedef struct AdcReading_s AdcReading;

#endif /* ADC_READING_H */