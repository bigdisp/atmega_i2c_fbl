/*
 * hex_parser.h
 *
 * Streaming Intel HEX parser for the FBL. Characters are fed one by one,
 * every record is checked against its checksum before its data is used, and
 * complete flash pages are written via flash_write_page().
 */

#ifndef SRC_HEX_PARSER_H_
#define SRC_HEX_PARSER_H_

#include <stdint.h>

#ifdef __AVR__
#include <avr/io.h>
#endif

#ifndef FBL_START_ADDR
/// First byte address of the FBL itself (must match -Ttext of the linker).
/// Data at or above this address is rejected, so the FBL can't overwrite itself.
#define FBL_START_ADDR 0x1800
#endif

/// Max. number of data bytes per record (avr-objcopy writes 16)
#define HEX_MAX_DATA 32

enum hex_parser_result
{
	e_hex_busy,     ///< More data needed
	e_hex_finished, ///< End of file record received, all data written
	e_hex_error     ///< Invalid input; parsing must be restarted with hex_parser_init()
};

/**
 * Reset the parser. Must be called before a new file is fed.
 *
 * Erases flash page 0, so the old application counts as gone until the new
 * one has been written completely (page 0 is written last, after the EOF record).
 */
void hex_parser_init(void);

/**
 * Feed one character of the hex file.
 *
 * Characters outside of records (line breaks, padding) are ignored.
 * Records must be in ascending address order (as written by avr-objcopy).
 */
enum hex_parser_result hex_parser_put(uint8_t c);

/**
 * Write one flash page. Implemented in flash.c (or by a unit test).
 * @param addr byte address of the page (page aligned)
 * @param content SPM_PAGESIZE bytes
 */
void flash_write_page(uint16_t addr, const uint8_t *content);

#endif /* SRC_HEX_PARSER_H_ */
