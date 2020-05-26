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
#define CONFIG_ADC_TMR 1000000000  // 1 or 1Hz
/* By how many bits should the 12Bit ADC-values be padded? */
#define CONFIG_ADC_ENCODING 4  // 16bit