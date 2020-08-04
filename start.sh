#!/bin/bash

WORK_DIR="/home/lfd/Projects/projektarbeit-lfd-fast-adc"

until test -e /dev/uio5; do sleep 1; done
until test -e /sys/devices/platform/libpruio/state; do sleep 1; done

$WORK_DIR/projektarbeit-lfd-fast-adc -c $WORK_DIR/user_lfd-fast-adc.conf
