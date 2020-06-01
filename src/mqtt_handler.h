/********************************************************************************
 * mqtt_handler.h
 ********************************************************************************/
#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

/* ***** Connection state ***** */

/* 0 - initialized, 1 - connecting, 2 - connected, 3 - connection failed, 4 - ungracefully
 * disconnected, 5 - gracefully disconnected */
typedef enum {
  MQTT_CON_INITIALIZED,
  MQTT_CON_CONNECTING,
  MQTT_CON_CONNECTED,
  MQTT_CON_CONNECTION_FAILED,
  MQTT_CON_UNGRACEFUL_DISCONNECT,
  MQTT_CON_GRACEFUL_DISCONNECT
} MqttConnectionState;

/* Globally availabe MQTT connection state */
extern volatile MqttConnectionState mqtt_connection_flag;

/* ***** Actual exported functions ***** */

/* MQTT handler to be called as child thread */
void* mqtt_handler(void* arg);

#endif /* MQTT_HANDLER_H */