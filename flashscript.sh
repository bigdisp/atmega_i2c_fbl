#!/bin/bash

# Flash an intel hex file via i2c to an atmega8 or atmega88pa device
# running atmega_i2c_fbl (e.g. from a raspberry pi).
#
# Note that the correct FBL must be present on the device and listen.
# A reset command will be written to the application to send it back
# to FBL mode. The reset command written is 0x10 0xFB 0x4C (0x10 "FBL").
#
# Flashing is initiated by sending 'wf' to the device. After that, the hex
# file is sent to the FBL in chunks of 32 characters. The FBL parses and
# verifies each record itself. After flashing, the app is started.
#
# This used to call i2cset/i2cget once per hex character with a 250ms
# sleep in between (hours for a full image). The actual work is now done
# by fbl_flash.py, which talks to /dev/i2c-* directly and only waits as
# long as the FBL is really busy.

function help() {
	echo "$0: Usage"
	echo -e "\t$0 <i2c address> <flash file> [fbl_flash.py options]"
	echo -e "\tExample:"
	echo -e "\t$0 0x10 file.hex"
}

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
shift 2

exec python3 "$(dirname "$0")/fbl_flash.py" "$I2C_ADD" "$flashfile" "$@"
