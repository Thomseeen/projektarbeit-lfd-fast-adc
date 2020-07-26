# Super fast BeagleBone Black ADC
Supposed to be part of my "Projektarbeit" this tool should be able to sample the BBB's ADC-channels in a very tightly timed manner.
Analog readings will then be published using MQTT (and maybe to shared memory for other C/C++ application on the same host).
This tool is supposed to be CLI-only and configured via config-files.

Libraries in use:
* DTJF's libpruio (should be installed as global lib) - https://github.com/DTJF/libpruio
* exlipse's paho.mqtt.c (should be installed globally) - https://github.com/eclipse/paho.mqtt.c
* martinh's libconfuse (should be installed as global lib) - https://github.com/martinh/libconfuse
* rxi's log.c (git submodul, symlinked) - https://github.com/rxi/log.c
* chrismerch's rpa_queue adapted from Apache apr_queue (git submodul, symlinked) - https://github.com/chrismerck/rpa_queue