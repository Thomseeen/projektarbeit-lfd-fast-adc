#include "mqtt_handler.h"

#include <stdlib.h>
#include <unistd.h>

#include "common.h"

volatile MqttConnectionState mqtt_connection_flag = MQTT_CON_INITIALIZED;

/********************************************************************************
 * MQTTAsync callbacks declaration for local use
 ********************************************************************************/
void mqtt_handler_delivered(void* context, MQTTAsync_token token);
int mqtt_handler_msg_arrived(void* context, char* topicName, int topicLen, MQTTAsync_message* m);
void mqtt_handler_connlost(void* context, char* cause);
void mqtt_handler_on_disconnect(void* context, MQTTAsync_successData* response);
void mqtt_handler_on_connect(void* context, MQTTAsync_successData* response);
void mqtt_handler_on_connect_failure(void* context, MQTTAsync_failureData* response);

/********************************************************************************
 * Custom functions
 ********************************************************************************/

int mqtt_handler_connect(MQTTAsync client) {
    /* Build connection options */
    MQTTAsync_connectOptions mqtt_conn_opts = MQTTAsync_connectOptions_initializer;
    mqtt_conn_opts.keepAliveInterval = MQTT_KEEP_ALIVE;
    mqtt_conn_opts.cleansession = 1;

    /* MQTT paho async C client callbacks for connection */
    mqtt_conn_opts.onSuccess = mqtt_handler_on_connect;
    mqtt_conn_opts.onFailure = mqtt_handler_on_connect_failure;

    mqtt_conn_opts.context = client;

    /* Try to connect to MQTT-broker */
    int rc;
    log_info("Trying to connect...");
    mqtt_connection_flag = MQTT_CON_CONNECTING;
    if ((rc = MQTTAsync_connect(client, &mqtt_conn_opts)) != MQTTASYNC_SUCCESS) {
        return rc;
    }
    return MQTTASYNC_SUCCESS;
}

int mqtt_handler_send_measurement(void* context, AdcReading adc_reading) {
    /* Setup MQTT paho async C client structure */
    MQTTAsync mqtt_client = (MQTTAsync)context;
    MQTTAsync_responseOptions mqtt_conn_opts = MQTTAsync_responseOptions_initializer;
    MQTTAsync_message mqtt_pubmsg = MQTTAsync_message_initializer;

    /* Build message structure */
    mqtt_conn_opts.context = mqtt_client;
    mqtt_conn_opts.context = &adc_reading.mqtt_token;

    mqtt_pubmsg.payload = &adc_reading;
    mqtt_pubmsg.payloadlen = sizeof(adc_reading);

    mqtt_pubmsg.qos = MQTT_DEFAULT_QOS;
    mqtt_pubmsg.retained = 0;

    int rc;
    if ((rc = MQTTAsync_sendMessage(mqtt_client, MQTT_DEFAULT_TOPIC_PREFIX, &mqtt_pubmsg,
                                    &mqtt_conn_opts)) != MQTTASYNC_SUCCESS) {
        log_error("Failed to start sendMessage, return code %d", rc);
        return MQTTASYNC_FAILURE;
    }
    log_trace("Sent measurement from pin %hhu with seq_no %llu and %d bytes by token %d",
              adc_reading.pin_no, adc_reading.seq_no, sizeof(adc_reading), adc_reading.mqtt_token);
    return MQTTASYNC_SUCCESS;
}

/********************************************************************************
 * Main handler function
 ********************************************************************************/
void* mqtt_handler(void* arg) {
    init_logger();

    log_info("MQTT Handler thread has been started");
    /* Setup MQTT paho async C client structure */
    MQTTAsync mqtt_client;

    /* Initialize MQTT paho async C client incl. callbacks */
    int rc;
    if ((rc = MQTTAsync_create(&mqtt_client, MQTT_BROKER_ADDRESS, MQTT_CLIENTID,
                               MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTASYNC_SUCCESS) {
        mqtt_connection_flag = MQTT_CON_CONNECTION_FAILED;
        log_fatal("... Failed to create MQTT-client, return code %d", rc);
        log_info("MQTT Handler thread terminates");
        exit(EXIT_FAILURE);
    };

    if ((rc = MQTTAsync_setCallbacks(mqtt_client, mqtt_client, mqtt_handler_connlost,
                                     mqtt_handler_msg_arrived, mqtt_handler_delivered)) !=
        MQTTASYNC_SUCCESS) {
        mqtt_connection_flag = MQTT_CON_CONNECTION_FAILED;
        log_fatal("... Failed to setup callbacks in MQTT-client, return code %d", rc);
        log_info("MQTT Handler thread terminates");
        exit(EXIT_FAILURE);
    };

    if ((rc = mqtt_handler_connect(mqtt_client)) != 0) {
        mqtt_connection_flag = MQTT_CON_CONNECTION_FAILED;
        log_fatal("... Failed to start connection, return code %d", rc);
        log_info("MQTT Handler thread terminates");
        exit(EXIT_FAILURE);
    };

    /* Main loop */
    while (1) {
        /* Index to keep track were the last value-to-be-sent was found */
        uint32_t host_adc_buffer_indexer = 0;
        /* Connection established - checking for new data */
        if (mqtt_connection_flag == MQTT_CON_CONNECTED) {
            pthread_mutex_lock(&measurements_buffer_lock);
            for (; host_adc_buffer_indexer < CONFIG_HOST_ADC_BUFFER_SIZE;
                 host_adc_buffer_indexer++) {
                if (measurements_buffer[host_adc_buffer_indexer].status == ADC_READ_NEW_VALUE) {
                    measurements_buffer[host_adc_buffer_indexer].status = ADC_READ_GRABBED_FROM_RB;
                    break;
                }
            }
            /* If no value-to-be-sent was found, start over */
            if (host_adc_buffer_indexer == CONFIG_HOST_ADC_BUFFER_SIZE) {
                host_adc_buffer_indexer = 0;
            }
            /* New value found, pass to send-function */
            if (measurements_buffer[host_adc_buffer_indexer].status == ADC_READ_GRABBED_FROM_RB) {
                measurements_buffer[host_adc_buffer_indexer].status = ADC_READ_SENDING;
                if (mqtt_handler_send_measurement(mqtt_client,
                                                  measurements_buffer[host_adc_buffer_indexer])) {
                    /* Could not send because of disconnect, try again */
                    log_info("Reset measurement flag to ADC_READ_NEW_VALUE to retry sending");
                    measurements_buffer[host_adc_buffer_indexer].mqtt_token = ADC_READ_NEW_VALUE;
                }
            }
            pthread_mutex_unlock(&measurements_buffer_lock);
        } else {
            /* No connection */
            ;
        }
    }
    log_info("MQTT Handler thread terminates");
    MQTTAsync_destroy(&mqtt_client);
    exit(rc);
}

/********************************************************************************
 * MQTTAsync callbacks definitions for local use
 ********************************************************************************/

void mqtt_handler_delivered(void* context, MQTTAsync_token token) {
    log_trace("Message with token value %d delivery confirmed", token);
}

int mqtt_handler_msg_arrived(void* context, char* topicName, int topicLen, MQTTAsync_message* m) {
    /* not expecting any messages */
    return 1;
}

void mqtt_handler_connlost(void* context, char* cause) {
    mqtt_connection_flag = MQTT_CON_UNGRACEFUL_DISCONNECT;

    /* Setup MQTT paho async C client structure again */
    MQTTAsync mqtt_client = (MQTTAsync)context;

    /* Reinitialize MQTT paho async C client */
    log_error("MQTT connection lost, cause: %s", cause);
    log_info("Trying to reconnect...");

    int rc;
    if ((rc = mqtt_handler_connect(mqtt_client)) != 0) {
        mqtt_connection_flag = MQTT_CON_CONNECTION_FAILED;
        log_fatal("... Failed to start reconnection, return code %d", rc);
        log_info("MQTT Handler thread terminates");
        exit(EXIT_FAILURE);
    };
}

void mqtt_handler_on_disconnect(void* context, MQTTAsync_successData* response) {
    mqtt_connection_flag = MQTT_CON_GRACEFUL_DISCONNECT;
    log_info("Gracefully disconnected from MQTT-broker");
}

void mqtt_handler_on_connect(void* context, MQTTAsync_successData* response) {
    mqtt_connection_flag = MQTT_CON_CONNECTED;
    log_info("... Successfully connected");
}

void mqtt_handler_on_connect_failure(void* context, MQTTAsync_failureData* response) {
    mqtt_connection_flag = MQTT_CON_CONNECTION_FAILED;
    MQTTAsync mqtt_client = (MQTTAsync)context;

    log_warn("Failed during connection to MQTT-broker, return code %d",
             response ? response->code : 0);
    log_info("Retrying in %d seconds", MQTT_RECONNECT_TIMER);
    sleep(MQTT_RECONNECT_TIMER);

    int rc;
    if ((rc = mqtt_handler_connect(mqtt_client)) != 0) {
        mqtt_connection_flag = MQTT_CON_CONNECTION_FAILED;
        log_fatal("... Failed to start reconnection, return code %d", rc);
        log_info("MQTT Handler thread terminates");
        exit(EXIT_FAILURE);
    };
}