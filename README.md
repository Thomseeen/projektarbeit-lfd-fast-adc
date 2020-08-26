# Super fast BeagleBone Black ADC
Part of my "Projektarbeit" this tool is able to sample the BBB's ADC-channels in a very tightly timed manner.
Analog readings will then be published using MQTT. This tool is CLI-only and configured via config-files.
The provided unit file expects a copy of `example_lfd-fast-adc.conf` as `user_lfd-fast-adc.conf` to be provided.

Libraries in use:
* DTJF's libpruio (should be installed as global lib - from package) - https://github.com/DTJF/libpruio
* exlipse's paho.mqtt.c (should be installed as global lib) - https://github.com/eclipse/paho.mqtt.c
* martinh's libconfuse (should be installed as global lib) - https://github.com/martinh/libconfuse
* rxi's log.c (git submodul, symlinked) - https://github.com/rxi/log.c
* chrismerch's rpa_queue adapted from Apache apr_queue (git submodul, symlinked) - https://github.com/chrismerck/rpa_queue
