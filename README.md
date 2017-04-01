Atmega8(8pa) I2C Flash Bootloader
=====================================
This repository provides a simple flash bootloader for flashing an atmega8 or atmega88pa via the i2c interface.
Warning: Development is at an early stage and the FBL may not work as intended. There is currently no verification method to verify the written flash. Also, the FBL is currently very large.

The FBL takes ascii hex over the i2c interface, converts them to binary values and stores them in RAM. Once a full page is received, it is written to the flash. After that, the controller will accept more data to be written until the full file has been written.

Currently, the FBL stays in bootmode after system start. A special i2c command must be sent to leave the FBL and jump to the application. 
 
Requirements:
-------------
 * Atmega8, Atmega88p(a) or similar (other microcontrollers may be suitable if the code is adjusted accordingly),
 * A suitable flashing device,
 * A suitable i2c controller, such as the raspberry pi with `i2c-tools` installed or similar.
 * Correct fuse settings (1024 words for the FBL, FBL enabled, 8MHz clock)

Build:
------
Navigate to the release folder and run `make`.

Usage:
------
The default i2c address of the atmega88p is `0x14`. The device takes either no parameters with the command, 8 bits of data or 16 bits of data. Several commands are available:

 * `0x73` or `0x53` (s or S): The FBL jumps to the application. No verification of the application is performed.
 * `0x77 0x66` (wf): The FBL goes into flashing mode. 0x77 is the address, 0x66 the value to be sent.

When in flashing mode, the FBL accepts only single byte writes (only address, no data). The contents of the hex file must be provided sequentially with enough time between each transmit to allow the FBL to write the pages as necessary. The atmega8 has a page size of 64 bytes, therefore a longer break should be put in between transmissions after 64 flash bytes (overhead must be subtracted). There is no way of leaving flashing mode unless an error occurrs or flashing is finished.
In flashing mode, the FBL will send back every received byte. By reading a single byte (no address, direct read) the received data can be verified.

An example flash sequence is provided in the flashscript.sh shell script intended for the raspberry pi.

License
-------
See [License.md](License.md).
