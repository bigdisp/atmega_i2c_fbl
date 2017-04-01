/*
 * flash.h
 *
 *  Created on: 26.02.2017
 *      Author: martin
 */

#ifndef SRC_FLASH_H_
#define SRC_FLASH_H_

#define FLASH_PARSER_SYM_LINE_START ':'

#include <avr/boot.h>

void flash_write_page(uint32_t page, uint8_t *content);
uint16_t flash_get_pagesize();
uint16_t flash_ascii_to_num(const uint8_t * ascii, uint8_t num);

enum flash_parser_state {
	e_parser_state_start = 1,
	e_parser_state_size,
	e_parser_state_address,
	e_parser_state_type,
	e_parser_state_data,
	e_parser_state_data_written,
	e_parser_state_checksum,
	e_parser_state_checksum_data_written,
	e_parser_state_error,
	e_parser_state_finished
};

enum flash_parser_line_type {
	e_parser_line_type_data = 0,
	e_parser_line_type_checksum = 1,
};

#endif /* SRC_FLASH_H_ */
