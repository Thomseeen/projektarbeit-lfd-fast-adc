#include "mqtt_handler.h"

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
    mqtt_conn_opts.keepAliveInterval = cfg_getint(cfg, "MQTT_KEEP_ALIVE");
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

int mqtt_handler_send_measurement(void* context, AdcReading* adc_reading) {
    /* Set status to sending */
    adc_reading->status = ADC_READ_SENDING;

    /* Setup MQTT paho async C client structure */
    MQTTAsync mqtt_client = (MQTTAsync)context;
    MQTTAsync_responseOptions mqtt_conn_opts = MQTTAsync_responseOptions_initializer;
    MQTTAsync_message mqtt_pubmsg = MQTTAsync_message_initializer;

    /* Build message structure */
    mqtt_conn_opts.context = mqtt_client;

    mqtt_pubmsg.payload = adc_reading;
    mqtt_pubmsg.payloadlen = sizeof(*adc_reading);

    mqtt_pubmsg.qos = cfg_getint(cfg, "MQTT_DEFAULT_QOS");
    mqtt_pubmsg.retained = 0;

    int rc;
    if ((rc = MQTTAsync_sendMessage(mqtt_client, cfg_getstr(cfg, "MQTT_DEFAULT_TOPIC_PREFIX"),
                                    &mqtt_pubmsg, &mqtt_conn_opts)) != MQTTASYNC_SUCCESS) {
        log_error("Failed to start sendMessage, return code %d", rc);
        return MQTTASYNC_FAILURE;
    }

#if L_TRACE
    log_trace("Sent measurement from pin %hhu with seq_no %llu and %d bytes", adc_reading->pin_no,
              adc_reading->seq_no, sizeof(*adc_reading));
#endif

    /* Free memory */
    free(adc_reading);

    return MQTTASYNC_SUCCESS;
}

/********************************************************************************
 * Main handler function
 ********************************************************************************/
void* mqtt_handler(void* arg) {
    log_info("MQTT Handler thread has been started");
    /* Setup MQTT paho async C client structure */
    MQTTAsync mqtt_client;

    /* Initialize MQTT paho async C client incl. callbacks */
    int rc;
    if ((rc = MQTTAsync_create(&mqtt_client, cfg_getstr(cfg, "MQTT_BROKER_ADDRESS"),
                               cfg_getstr(cfg, "MQTT_CLIENTID"), MQTTCLIENT_PERSISTENCE_NONE,
                               NULL)) != MQTTASYNC_SUCCESS) {
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
        /* Connection established - checking for new data */
        if (mqtt_connection_flag == MQTT_CON_CONNECTED) {
            /* Some data holders */
            AdcReading* adc_reading_p = NULL;  // Pointer to measurement

            /* Grab data or block if non available */
            if (rpa_queue_size(measurements_queue)) {
                if (!rpa_queue_pop(measurements_queue, (void**)&adc_reading_p)) {
                    log_fatal("Error on popping new measurement from queue. Exiting...");
                    log_info("MQTT Handler thread terminates");
                    MQTTAsync_destroy(&mqtt_client);
                    exit(rc);
                }
                adc_reading_p->status = ADC_READ_GRABBED_FROM_QUEUE;

                /* New value found, pass to send-function */
                if (adc_reading_p->status == ADC_READ_GRABBED_FROM_QUEUE) {
                    if (mqtt_handler_send_measurement(mqtt_client, adc_reading_p) !=
                        MQTTASYNC_SUCCESS) {
                        /* Could not send because of disconnect, try again */
#if L_WARN
                        log_warn(
                            "Reset measurement flag to ADC_READ_NEW_VALUE and push back into queue "
                            "to "
                            "retry sending");
#endif
                        adc_reading_p->status = ADC_READ_NEW_VALUE;
                        if (!rpa_queue_push(measurements_queue, adc_reading_p)) {
                            log_error("Error on pushing measurement back into queue, discard");
                            free(adc_reading_p);
                        }
                    }
                }
            }
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
    /* No delivery confirmation on QoS 0 */
#if L_TRACE
    log_trace("Message with token value %d delivery confirmed", token);
#endif
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
    log_info("Retrying in %d seconds", cfg_getint(cfg, "MQTT_RECONNECT_TIMER"));
    sleep(cfg_getint(cfg, "MQTT_RECONNECT_TIMER"));

    int rc;
    if ((rc = mqtt_handler_connect(mqtt_client)) != 0) {
        mqtt_connection_flag = MQTT_CON_CONNECTION_FAILED;
        log_fatal("... Failed to start reconnection, return code %d", rc);
        log_info("MQTT Handler thread terminates");
        exit(EXIT_FAILURE);
    };
}