/********************************************************************************
 * config.h
 ********************************************************************************/
#ifndef CONFIG_H
#define CONFIG_H

/********************************************************************************
 * CONFIGURABLE PART
 ********************************************************************************/

/* Define whether PRU-0 or PRU-1 should be used */
#define CONFIG_PRU_NO 1
/* Define whether ADC subsystem should be used */
#define CONFIG_SUBSYSTEM_ADC 1

/* Steps to average ADC readings 1, 2, 4, 8 or 16 */
#define CONFIG_ADC_AVERAGING_STEPS 1
/* Delay cycles between each sample - cycles @ 24MHz ~= 42ns - 0..255*/
#define CONFIG_ADC_SAMPLE_DELAY 24  // ~1us
/* Delay bevore every averaging sequence - cycles @ 24MHz ~= 42ns - 0..0x3FFFF */
#define CONFIG_ADC_OPEN_DELAY 238  // ~10us
/* Sampling rate as Tmr [ns] between samples */
#define CONFIG_ADC_TMR 1000000000  // 1s or 1Hz
/* By how many bits should the 12Bit ADC-values be padded? */
#define CONFIG_ADC_ENCODING 4  // 16bit
/* Bit mask for active AIN - LSB is charge up -> ignore, Bit 1 is AIN0, Bit 2 is AIN1, ... */
#define CONFIG_ADC_PIN_MASK 0b000100110
/* How many samples to fetch in one go per active AIN before ring buffer wrap around */
#define CONFIG_ADC_RB_SAMPLES_PER_PORT 3

/* How big the host buffer of ADC-readings should be */
#define CONFIG_HOST_ADC_BUFFER_SIZE 20

/********************************************************************************
 * FIXED DEFINES
 ********************************************************************************/
/* Maximum samples the PRUSS drivers memory can hold for the ADC's ring buffer */
#define LFD_MAX_ADC_BUFFER_SAMPLES 128000
/* Maximum available AIN count */
#define LFD_MAX_ADC_PINS 8

#endif /* CONFIG_H */