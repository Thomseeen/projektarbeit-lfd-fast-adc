#include "mqtt_handler.h"

#include <stdlib.h>
#include <unistd.h>

#include "common.h"

volatile MqttConnectionState mqtt_connection_flag = MQTT_CON_INITIALIZED;

void mqtt_handler_on_send(void* context, MQTTAsync_successData* response) {
  log_debug("Message with token value %d delivery confirmed", response->token);
}

void mqtt_handler_on_connect(void* context, MQTTAsync_successData* response) {
  mqtt_connection_flag = MQTT_CON_CONNECTED;
  log_info("... Successfully connected");
}

void mqtt_handler_on_connect_failure(void* context, MQTTAsync_failureData* response) {
  mqtt_connection_flag = MQTT_CON_CONNECTION_FAILED;
  log_fatal("Failed during connection to MQTT-broker, return code %d",
            response ? response->code : 0);
}

void mqtt_handler_connlost(void* context, char* cause) {
  mqtt_connection_flag = MQTT_CON_UNGRACEFUL_DISCONNECT;
  /* Setup MQTT paho async C client structure again */
  MQTTAsync mqtt_client = (MQTTAsync)context;
  MQTTAsync_connectOptions mqtt_conn_opts = MQTTAsync_connectOptions_initializer;

  /* Reinitialize MQTT paho async C client */
  int rc;
  log_info("MQTT connection lost, cause: %s", cause);
  log_info("Trying to reconnect...");

  mqtt_conn_opts.keepAliveInterval = MQTT_KEEP_ALIVE;
  mqtt_conn_opts.cleansession = 1;

  /* Try to reconnect to MQTT-broker */
  mqtt_connection_flag = MQTT_CON_CONNECTING;
  if ((rc = MQTTAsync_connect(mqtt_client, &mqtt_conn_opts)) != MQTTASYNC_SUCCESS) {
    mqtt_connection_flag = MQTT_CON_CONNECTION_FAILED;
    log_fatal("... Failed to reconnect to MQTT-broker, return code %d", rc);
    exit(EXIT_FAILURE);
  }
}

void mqtt_handler_on_disconnect(void* context, MQTTAsync_successData* response) {
  mqtt_connection_flag = MQTT_CON_GRACEFUL_DISCONNECT;
  log_info("Gracefully disconnected from MQTT-broker");
}

void mqtt_handler_send_measurement(void* context, AdcReading* adc_reading) {
  /* Setup MQTT paho async C client structure */
  MQTTAsync mqtt_client = (MQTTAsync)context;
  MQTTAsync_responseOptions mqtt_conn_opts = MQTTAsync_responseOptions_initializer;
  MQTTAsync_message mqtt_pubmsg = MQTTAsync_message_initializer;

  /* Build message structure */
  int rc;
  mqtt_conn_opts.onSuccess = mqtt_handler_on_send;
  mqtt_conn_opts.context = mqtt_client;

  mqtt_pubmsg.payload = adc_reading;
  mqtt_pubmsg.payloadlen = sizeof(*adc_reading);
  log_trace("Sending measurement from pin %hhu with seq_no %llu and %d bytes", adc_reading->pin_no,
            adc_reading->seq_no, sizeof(*adc_reading));
  mqtt_pubmsg.qos = MQTT_DEFAULT_QOS;
  mqtt_pubmsg.retained = 0;

  if ((rc = MQTTAsync_sendMessage(mqtt_client, MQTT_DEFAULT_TOPIC_PREFIX, &mqtt_pubmsg,
                                  &mqtt_conn_opts)) != MQTTASYNC_SUCCESS) {
    log_error("Failed to start sendMessage, return code %d", rc);
    exit(EXIT_FAILURE);
  }
}

void* mqtt_handler(void* arg) {
  init_logger();

  log_info("MQTT Handler thread has been started");
  /* Setup MQTT paho async C client structure */
  MQTTAsync mqtt_client;
  MQTTAsync_connectOptions mqtt_conn_opts = MQTTAsync_connectOptions_initializer;

  /* Initialize MQTT paho async C client */
  int rc;
  MQTTAsync_create(&mqtt_client, MQTT_BROKER_ADDRESS, MQTT_CLIENTID, MQTTCLIENT_PERSISTENCE_NONE,
                   NULL);
  MQTTAsync_setCallbacks(mqtt_client, NULL, mqtt_handler_connlost, NULL, NULL);

  mqtt_conn_opts.keepAliveInterval = 20;
  mqtt_conn_opts.cleansession = 1;

  /* MQTT paho async C client callbacks */
  mqtt_conn_opts.onSuccess = mqtt_handler_on_connect;
  mqtt_conn_opts.onFailure = mqtt_handler_on_connect_failure;
  mqtt_conn_opts.context = mqtt_client;

  /* Try to connect to MQTT-broker */
  log_info("Trying to connect...");
  mqtt_connection_flag = MQTT_CON_CONNECTING;
  if ((rc = MQTTAsync_connect(mqtt_client, &mqtt_conn_opts)) != MQTTASYNC_SUCCESS) {
    mqtt_connection_flag = MQTT_CON_CONNECTION_FAILED;
    log_fatal("... Failed to connect to MQTT-broker, return code %d", rc);
    log_info("MQTT Handler thread terminates");
    exit(EXIT_FAILURE);
  }

  /* Wait for connection */
  while (mqtt_connection_flag != MQTT_CON_CONNECTED) {
    ;
  }

  /* Main loop checking for new data */
  AdcReading adc_reading;
  while (mqtt_connection_flag == MQTT_CON_CONNECTED) {
    pthread_mutex_lock(&measurements_buffer_lock);
    for (int ii = 0; ii < CONFIG_HOST_ADC_BUFFER_SIZE; ii++) {
      if (measurements_buffer[ii].status == ADC_READ_NEW_VALUE) {
        measurements_buffer[ii].status = ADC_READ_GRABBED_FROM_RB;
        adc_reading = measurements_buffer[ii];
        break;
      }
    }
    pthread_mutex_unlock(&measurements_buffer_lock);
    if (adc_reading.status == ADC_READ_GRABBED_FROM_RB) {
      mqtt_handler_send_measurement(mqtt_client, &adc_reading);
      adc_reading.status = ADC_READ_SENDING;
    }
  }

  log_info("MQTT Handler thread terminates");
  MQTTAsync_destroy(&mqtt_client);
  pthread_exit(&rc);
}