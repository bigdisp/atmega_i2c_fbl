#!/bin/bash
set -x
DEVICE=atmega8
PROGRAMMER_FLAGS="-P usb -c avrisp2 -p ${DEVICE}"
# WARNING: FUSES set are for atmega8! Do not set anything else with this!
# Original Fuses: avrdude: safemode: Fuses OK (H:FF, E:D9, L:E1)

# Fuse Settings:
# Internal Oscillator 1 MHz
# Brownout detection 2.7 V
# Boot Reset Vector Enabled, Start: 0E00 (512 words size)
# Preserve EEprom through Chip Erase
# Serial program downloading enabled
#avrdude ${PROGRAMMER_FLAGS} -U lfuse:w:0xa1:m -U hfuse:w:0xd2:m 

# Fuse Settings:
# Internal Oscillator 1 MHz
# Brownout detection 2.7 V
# Boot Reset Vector Enabled, Start: 0C00 (1024 words size)
# Preserve EEprom through Chip Erase
# Serial program downloading enabled
#avrdude ${PROGRAMMER_FLAGS} -U lfuse:w:0xa1:m -U hfuse:w:0xd0:m 

# Fuse Settings:
# Internal Oscillator 8 MHz 64 ms delay
# Brownout detection 2.7 V
# Boot Reset Vector Enabled, Start: 0C00 (1024 words size)
# Preserve EEprom through Chip Erase
# Serial program downloading enabled
avrdude ${PROGRAMMER_FLAGS} -U lfuse:w:0xa4:m -U hfuse:w:0xd0:m 
