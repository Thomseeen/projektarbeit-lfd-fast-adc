#!/bin/bash -v

gcc -O3 -Wall -DLOG_USE_COLOR -o obj/common.o -c src/common.c
gcc -O3 -Wall -DLOG_USE_COLOR -o obj/log.o -c src/log.c
gcc -O3 -Wall -DLOG_USE_COLOR -o obj/mqtt_handler.o -c src/mqtt_handler.c
gcc -Wall -DLOG_USE_COLOR -o obj/main.o -c src/main.c
gcc -O3 -Wall -DLOG_USE_COLOR -o projektarbeit-lfd-fast-adc obj/common.o obj/log.o obj/mqtt_handler.o obj/main.o -l paho-mqtt3a -lpruio -lpthread