Atmega8(8pa) I2C Flash Bootloader
=====================================
This repository provides a simple flash bootloader for flashing an atmega8 or atmega88pa via the i2c interface.
Warning: Development is at an early stage and the FBL may not work as intended. There is currently no read back of the written flash.

The FBL takes an Intel HEX file (ascii) over the i2c interface. Every record is checked against its checksum before it is used, the data is collected in RAM and written to the flash one page at a time. Data for the FBL area (0x1800 and above) is rejected.

Startup behaviour:

 * After reset, the FBL waits 1 s for the flash command (`wf`). Without it, the application is started. Other i2c traffic does not delay the application start.
 * If there is no application (flash page 0 erased), the FBL stays active.
 * When flashing starts, page 0 is erased first and written last. An interrupted or failed update therefore never leaves a half-written application that would be started; the FBL stays active until a complete file was flashed.
 * After a successful update, the new application is started immediately.
 * If the master stops for more than 2 s while flashing, the FBL aborts, releases the bus and answers `ERR`.
 * The watchdog is disabled on FBL start (it stays enabled after a watchdog reset on the atmega88), so the application can use a watchdog reset to enter the FBL.
 
Requirements:
-------------
 * Atmega8, Atmega88p(a) or similar (other microcontrollers may be suitable if the code is adjusted accordingly),
 * A suitable flashing device,
 * A suitable i2c controller, such as the raspberry pi with `i2c-tools` installed or similar.
 * Correct fuse settings (1024 words for the FBL, FBL enabled, 8MHz clock)

Build:
------
Navigate to the release folder and run `make`. The FBL is built for the atmega8 by default; for the atmega88(p)(a) change `-mmcu` in `Release/makefile` and `Release/src/*/subdir.mk`.

Usage:
------
The i2c address is read from EEPROM byte 0 (like the application); valid are `0x10` to `0x77`, otherwise `0x10` is used (the same default as the scoreboard application, so the FBL always listens on the address of the application). The device takes either no parameters with the command, 8 bits of data or 16 bits of data. Several commands are available:

 * `0x73` or `0x53` (s or S): The FBL jumps to the application (if there is one). No verification of the application is performed.
 * `0x77 0x66` (wf): The FBL goes into flashing mode. 0x77 is the address, 0x66 the value to be sent. A following single byte read returns `f`.

When in flashing mode, the FBL accepts writes of 1 to 32 characters of the hex file (line breaks may be omitted). Each write must be followed by a single byte read, which completes the exchange and returns the last character of the *previous* chunk (useful to detect errors). While the FBL parses a chunk or writes a flash page, it does not acknowledge its address, so the master just has to retry a NACKed write; no fixed delays are needed. There is no way of leaving flashing mode unless an error occurrs or flashing is finished (end-of-file record). On an error, the FBL stays active and answers `ERR`; flashing has to be restarted with `wf`.

Flashing from a raspberry pi (python3 only, no extra modules needed):

    ./fbl_flash.py 0x10 app.hex          # or: ./flashscript.sh 0x10 app.hex

This should take a few seconds for a full 6 KiB application (the old one-character-per-`i2cset` script needed well over an hour). See `./fbl_flash.py --help` for options.

The tool first sends `0x10 0x4C 0xFB` ("go to FBL") to the application, then repeats `wf` for up to 10 s until the FBL answers. The scoreboard application resets into the FBL on this command via the watchdog; this requires the BOOTRST fuse to be programmed (atmega88pa: efuse `0xF8` = 1024 words boot section + BOOTRST). If the application doesn't implement that command, reset/power-cycle the device while the tool is waiting.

Tests:
------
All tests run on a PC, no hardware required (needs gcc and python3; `make avr` also needs gcc-avr/avr-libc):

    make -C tests        # C tests: hex parser + flash.c on a simulated SPM flash (ASan/UBSan), then test_hex_parser.py
    make -C tests avr    # avr-gcc build of the FBL + check that it fits into the 2 KiB boot section

The hex parser alone can also be tested with `python3 tests/test_hex_parser.py`.

License
-------
See [License.md](License.md).
