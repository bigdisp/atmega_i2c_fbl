#!/bin/bash

# This script will use i2cset for flashing an intel hex file via i2c
# to an atmega8 or atmega88pa device.
# 
# Note that the correct FBL must be present on the device and listen.
# A reset command will be written to the application to send it back
# to FBL mode. 
#
# The reset command written is 0x10 0xFB 0x4C (0x10 "FBL").
# The FBL itself should react with a reset of the application start timer.
# 
# Flashing is initiated by sending 'wf' to the device. This should put the
# FBL into flash mode. After that, the hex file is sent to the FBL, which
# must parse and verify the file itself. After flashing, the app is booted.

# 0. Functions & Config
# Functions
function help() {
	echo "$0: Usage"
	echo -e "\t$0 <i2c address> <flash file>"
	echo -e "\tExample:"
	echo -e "\t$0 0x10 file.hex"
}

function to_hex() {
	retval=$(echo $1 | od -t x1 | grep 0000000 | sed "s/0000000 //")
	echo "${retval:0:2}"
}

# Config
I2C_ADD=$1
if [ "$I2C_ADD" == "" ]; then
	echo "$0: Error: i2c Address cannot be empty."
	help
	exit 1;
fi

flashfile=$2
if [ "$flashfile" == "" ]; then
	echo "$0: Cannot write empty file" 
	help
	exit 2;
fi

# 1. Step down from application
echo "Stepdown from application..."
i2cset -y 1 ${I2C_ADD} 0x10 0xFB4C w
sleep .5

# 2. Set to flash mode (wf)
echo "Set to flash mode..."
i2cset -y 1 ${I2C_ADD} 0x77 0x66
sleep .7

# 2.5 Getting rid of wrong data in buffers
# Do not write :, that is the line start marker
i2cset -y 1 ${I2C_ADD} 0x00
result=$(i2cget -y 1 ${I2C_ADD})
if [ "$result" != "0x00" ]; then
	sleep .3
	i2cset -y 1 ${I2C_ADD} 0x00
	i2cget -y 1 ${I2C_ADD}
fi

# 3. Read flash file
echo "Writing flash file..."
#while IFS='' read -r line || [[ -n "$line" ]]; do
#while IFS='' read -rn1 sym || [[ -n "$sym" ]]; do
while IFS='' read -rn1 sym ; do
	#echo "l: $sym"
	#echo -n "."
	#if [ "$sym" == "\n" -o "$sym" == "" ]; then
	#	echo "|"
	#fi
	hexval=$(to_hex "$sym")
	#echo "h: $hexval"
	i2cset -y 1 ${I2C_ADD} 0x$hexval
	sleep .25
	result=$(i2cget -y 1 ${I2C_ADD} | sed 's/0x//')
	echo -ne "\x$result"
	
	#echo -n "$sym"

done < "$flashfile"
