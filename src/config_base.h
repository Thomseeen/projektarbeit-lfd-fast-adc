/********************************************************************************
 * config.h
 ********************************************************************************/
#ifndef CONFIG_H
#define CONFIG_H

/********************************************************************************
 * Manifest
 ********************************************************************************/
#define VERSION "1.1.0"

/********************************************************************************
 * CONFIGURABLE PART
 ********************************************************************************/

/* ***** LOG LEVEL ***** */

/* Logger */
#define INFO
/* Paho MQTT */
//#define MQTT_C_CLIENT_TRACE on
//#define MQTT_C_CLIENT_TRACE_LEVEL PROTOCOL

/********************************************************************************
 * FIXED DEFINES
 ********************************************************************************/
/* Maximum samples the PRUSS drivers memory can hold for the ADC's ring buffer */
#define LFD_MAX_ADC_BUFFER_SAMPLES 128000
/* Maximum available AIN count */
#define LFD_MAX_ADC_PINS 8

/* ***** FOR LOGGING MASKING ***** */
#ifdef TRACE
#define LOG_LEVEL LOG_TRACE
#define L_TRACE 1
#define L_DEBUG 1
#define L_INFO 1
#define L_WARN 1
#else
#ifdef DEBUG
#define LOG_LEVEL LOG_DEBUG
#define L_TRACE 0
#define L_DEBUG 1
#define L_INFO 1
#define L_WARN 1
#else
#ifdef INFO
#define LOG_LEVEL LOG_INFO
#define L_TRACE 0
#define L_DEBUG 0
#define L_INFO 1
#define L_WARN 1
#else
#ifdef WARN
#define LOG_LEVEL LOG_WARN
#define L_TRACE 0
#define L_DEBUG 0
#define L_INFO 0
#define L_WARN 1
#else
#define LOG_LEVEL LOG_ERROR
#define L_TRACE 0
#define L_DEBUG 0
#define L_INFO 0
#define L_WARN 0
#endif /* WARN */
#endif /* INFO */
#endif /* DEBUG */
#endif /* TRACE */

#endif /* CONFIG_H */
