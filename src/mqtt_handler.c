#include "mqtt_handler.h"

#include "common.h"

volatile MqttConnectionState mqtt_connection_flag = MQTT_CON_INITIALIZED;

/********************************************************************************
 * MQTTAsync callbacks declaration for local use
 ********************************************************************************/
void mqtt_handler_delivered(void* context, MQTTClient_deliveryToken token);
int mqtt_handler_msg_arrived(void* context, char* topicName, int topicLen, MQTTClient_message* m);
void mqtt_handler_connlost(void* context, char* cause);

/********************************************************************************
 * Custom functions
 ********************************************************************************/

int mqtt_handler_connect(MQTTClient client) {
    /* Build connection options */
    MQTTClient_connectOptions mqtt_conn_opts = MQTTClient_connectOptions_initializer;
    mqtt_conn_opts.keepAliveInterval = MQTT_KEEP_ALIVE;
    mqtt_conn_opts.cleansession = 1;

    /* Try to connect to MQTT-broker */
    int rc;
    log_info("Trying to connect...");
    mqtt_connection_flag = MQTT_CON_CONNECTING;
    if ((rc = MQTTClient_connect(client, &mqtt_conn_opts)) != MQTTCLIENT_SUCCESS) {
        return rc;
    }
    return MQTTCLIENT_SUCCESS;
}

int mqtt_handler_send_measurement(void* context, AdcReading* adc_reading) {
    /* Setup MQTT paho async C client structure */
    MQTTClient mqtt_client = (MQTTClient)context;
    MQTTClient_message mqtt_pubmsg = MQTTClient_message_initializer;

    mqtt_pubmsg.payload = adc_reading;
    mqtt_pubmsg.payloadlen = sizeof(&adc_reading);

    mqtt_pubmsg.qos = MQTT_DEFAULT_QOS;
    mqtt_pubmsg.retained = 0;

    int rc;
    if ((rc = MQTTClient_publishMessage(mqtt_client, MQTT_DEFAULT_TOPIC_PREFIX, &mqtt_pubmsg,
                                        &adc_reading->mqtt_token)) != MQTTCLIENT_SUCCESS) {
        log_error("Failed to publish Message, return code %d", rc);
        return MQTTCLIENT_FAILURE;
    }
    log_trace("Sent measurement from pin %hhu with seq_no %llu and %d bytes by token %d",
              adc_reading->pin_no, adc_reading->seq_no, sizeof(&adc_reading),
              adc_reading->mqtt_token);
    return MQTTCLIENT_SUCCESS;
}

/********************************************************************************
 * Main handler function
 ********************************************************************************/
void* mqtt_handler(void* arg) {
    init_logger();

    log_info("MQTT Handler thread has been started");
    /* Setup MQTT paho async C client structure */
    MQTTClient mqtt_client;

    /* Initialize MQTT paho async C client incl. callbacks */
    int rc;
    if ((rc = MQTTClient_create(&mqtt_client, MQTT_BROKER_ADDRESS, MQTT_CLIENTID,
                                MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS) {
        mqtt_connection_flag = MQTT_CON_CONNECTION_FAILED;
        log_fatal("... Failed to create MQTT-client, return code %d", rc);
        log_info("MQTT Handler thread terminates");
        exit(EXIT_FAILURE);
    }
    mqtt_connection_flag = MQTT_CON_INITIALIZED;

    if ((rc = MQTTClient_setCallbacks(mqtt_client, mqtt_client, mqtt_handler_connlost,
                                      mqtt_handler_msg_arrived, mqtt_handler_delivered)) !=
        MQTTCLIENT_SUCCESS) {
        mqtt_connection_flag = MQTT_CON_CONNECTION_FAILED;
        log_fatal("... Failed to setup callbacks in MQTT-client, return code %d", rc);
        log_info("MQTT Handler thread terminates");
        exit(EXIT_FAILURE);
    }

    if ((rc = mqtt_handler_connect(mqtt_client)) != 0) {
        mqtt_connection_flag = MQTT_CON_CONNECTION_FAILED;
        log_fatal("... Failed to connect, return code %d", rc);
        log_info("MQTT Handler thread terminates");
        exit(EXIT_FAILURE);
    }
    mqtt_connection_flag = MQTT_CON_CONNECTED;
    log_info("... connected");

    /* Main loop */
    while (1) {
        /* Index to keep track were the last value-to-be-sent was found */
        uint32_t host_adc_buffer_indexer = 0;

        AdcReading adc_reading;
        AdcReading* adc_reading_pori;
        AdcReading* adc_reading_p = &adc_reading;
        /* Connection established - checking for new data */
        if (mqtt_connection_flag == MQTT_CON_CONNECTED) {
            pthread_mutex_lock(&measurements_buffer_lock);
            for (; host_adc_buffer_indexer < CONFIG_HOST_ADC_BUFFER_SIZE;
                 host_adc_buffer_indexer++) {
                if (measurements_buffer[host_adc_buffer_indexer].status == ADC_READ_NEW_VALUE) {
                    measurements_buffer[host_adc_buffer_indexer].status = ADC_READ_GRABBED_FROM_RB;
                    adc_reading_pori = &measurements_buffer[host_adc_buffer_indexer];
                    adc_reading = measurements_buffer[host_adc_buffer_indexer];
                    break;
                }
            }
            pthread_mutex_unlock(&measurements_buffer_lock);
            /* If no value-to-be-sent was found, start over */
            if (host_adc_buffer_indexer == CONFIG_HOST_ADC_BUFFER_SIZE) {
                host_adc_buffer_indexer = 0;
            }
            /* New value found, pass to send-function */
            if (adc_reading.status == ADC_READ_GRABBED_FROM_RB) {
                adc_reading.status = ADC_READ_SENDING;
                if (mqtt_handler_send_measurement(mqtt_client, adc_reading_p) !=
                    MQTTCLIENT_SUCCESS) {
                    /* Could not send because of disconnect, try again */
                    log_info("Reset measurement flag to ADC_READ_NEW_VALUE to retry sending");
                    pthread_mutex_lock(&measurements_buffer_lock);
                    adc_reading_pori->status = ADC_READ_NEW_VALUE;
                    pthread_mutex_unlock(&measurements_buffer_lock);
                } else {
                    pthread_mutex_lock(&measurements_buffer_lock);
                    adc_reading_pori->status = adc_reading.status;
                    adc_reading_pori->mqtt_token = adc_reading.mqtt_token;
                    pthread_mutex_unlock(&measurements_buffer_lock);
                }
            }
        } else {
            log_info("Waiting for connection");
        }
    }
    log_info("MQTT Handler thread terminates");
    MQTTClient_destroy(&mqtt_client);
    exit(rc);
}

/********************************************************************************
 * MQTTAsync callbacks definitions for local use
 ********************************************************************************/

void mqtt_handler_delivered(void* context, MQTTClient_deliveryToken token) {
    log_trace("Message with token value %d delivery confirmed", token);
    pthread_mutex_lock(&measurements_buffer_lock);
    for (int ii = 0; ii < CONFIG_HOST_ADC_BUFFER_SIZE; ii++) {
        if (measurements_buffer[ii].mqtt_token == token) {
            measurements_buffer[ii].status = ADC_READ_SENT;
        }
    }
    pthread_mutex_unlock(&measurements_buffer_lock);
}

int mqtt_handler_msg_arrived(void* context, char* topicName, int topicLen, MQTTClient_message* m) {
    /* not expecting any messages */
    return 1;
}

void mqtt_handler_connlost(void* context, char* cause) {
    mqtt_connection_flag = MQTT_CON_UNGRACEFUL_DISCONNECT;

    /* Setup MQTT paho async C client structure again */
    MQTTClient mqtt_client = (MQTTClient)context;

    /* Reinitialize MQTT paho async C client */
    log_error("MQTT connection lost, cause: %s", cause);
    log_info("Trying to reconnect...");

    int rc;
    while ((rc = mqtt_handler_connect(mqtt_client)) != 0) {
        mqtt_connection_flag = MQTT_CON_CONNECTION_FAILED;
        log_warn("... failed during reconnection, return code %d", rc);
        log_info("Retrying in %d seconds", MQTT_RECONNECT_TIMER);
        sleep(MQTT_RECONNECT_TIMER);
    }
    mqtt_connection_flag = MQTT_CON_CONNECTED;
    log_info("... reconnected");
}