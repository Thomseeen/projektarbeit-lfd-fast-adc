/********************************************************************************
 * mqtt_handler.h
 ********************************************************************************/
#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

/* 0 - initialized, 1 - connecting, 2 - connected, 3 - connection failed, 4 - ungracefully
 * disconnected, 5 - gracefully disconnected */
typedef enum {
  INITIALIZED,
  CONNECTING,
  CONNECTED,
  CONNECTION_FAILED,
  UNGRACEFUL_DISCONNECT,
  GRACEFUL_DISCONNECT
} MQTT_CONNECTION_STATE;

/* Globally availabe MQTT connection state */
extern volatile MQTT_CONNECTION_STATE mqtt_connection_flag;

/* MQTT handler to be called as child thread */
void* mqtt_handler(void* arg);

#endif /* MQTT_HANDLER_H */