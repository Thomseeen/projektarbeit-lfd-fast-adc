# Super fast BeagleBone Black ADC
Supposed to be part of my "Projektarbeit" this tool should be able to sample the BBB's ADC-channels in a very tightly timed manner.
Analog readings will then be published using MQTT (and maybe to shared memory for other C/C++ application on the same host).
This tool is supposed to be CLI-only and configured via config-files.

Libraries in use:
* DTJF's libpruio - https://github.com/DTJF/libpruio
* rxi's log.c - https://github.com/rxi/log.c
* exlipse's paho.mqtt.c - https://github.com/eclipse/paho.mqtt.c
