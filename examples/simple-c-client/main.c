#include <MQTTAsync.h>
#include <MQTTClient.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "adc_reading.h"

#define ADDRESS "tcp://192.168.178.16:1883"
#define CLIENTID "mfd"
#define TOPIC "lfd/#"
#define QOS 1
#define KEEP_ALIVE 30  // s

int msgarrvd(void* context, char* topicName, int topicLen, MQTTClient_message* message) {
  // printf("Message arrived topic: %s\n", topicName);

  AdcReading* adc_reading;
  adc_reading = message->payload;

  printf("Pin %hhu reads %f at %lu\n", adc_reading->pin_no,
         (float)adc_reading->value / (float)0xFFF0 * 1.8f, adc_reading->seq_no);

  MQTTClient_freeMessage(&message);
  MQTTClient_free(topicName);
  return 1;
}

void connlost(void* context, char* cause) {
  printf("Connection lost, cause: %s\n", cause);
}

int main(int argc, char* argv[]) {
  MQTTClient client;
  MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;

  int rc;
  if ((rc = MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL)) !=
      MQTTCLIENT_SUCCESS) {
    printf("Failed to create client, return code %d\n", rc);
    exit(EXIT_FAILURE);
  }

  if ((rc = MQTTClient_setCallbacks(client, NULL, connlost, msgarrvd, NULL)) !=
      MQTTCLIENT_SUCCESS) {
    printf("Failed to set callbacks, return code %d\n", rc);
    MQTTClient_destroy(&client);
    exit(EXIT_FAILURE);
  }

  conn_opts.keepAliveInterval = KEEP_ALIVE;
  conn_opts.cleansession = 1;

  if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
    printf("Failed to connect, return code %d\n", rc);
    MQTTClient_destroy(&client);
    exit(EXIT_FAILURE);
  }

  printf("Subscribing to topic %s\nfor client %s using QoS%d\n", TOPIC, CLIENTID, QOS);

  if ((rc = MQTTClient_subscribe(client, TOPIC, QOS)) != MQTTCLIENT_SUCCESS) {
    printf("Failed to subscribe, return code %d\n", rc);
    MQTTClient_destroy(&client);
    exit(EXIT_FAILURE);
  }

  while (1) {
    usleep(10000);
  }

  if ((rc = MQTTClient_disconnect(client, 10000)) != MQTTCLIENT_SUCCESS) {
    printf("Failed to disconnect, return code %d\n", rc);
    MQTTClient_destroy(&client);
    exit(EXIT_FAILURE);
  }

  MQTTClient_destroy(&client);
  exit(EXIT_SUCCESS);
}