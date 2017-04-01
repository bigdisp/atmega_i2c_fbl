/*
 * flash.c
 *
 *  Created on: 26.02.2017
 *      Author: martin
 */

#include "flash.h"
#include <avr/io.h>
#include <avr/interrupt.h>


void flash_write_page(uint32_t page, uint8_t *content)
{
	uint8_t sreg;
	uint16_t i;

	// Interrupt disable
	sreg = SREG;
	cli();

	// This combo could be replaced by boot_page_erase_safe
	eeprom_busy_wait();

	//Todo: Check whether we are trying to write a valid page
	boot_page_erase(page);

	boot_spm_busy_wait();

	// Fill the write buffer
	// Data is written 2 byte at a time, but addresses are on bytes
	// So we need to increment by two on each loop
	for (i = 0; i < SPM_PAGESIZE; i += 2)
	{
		uint16_t w = *content;
		++content;
		w += (*content) << 8;
		++content;

		boot_page_fill(page + i, w);
	}
	boot_page_write(page);
	boot_spm_busy_wait();

	boot_rww_enable();

	// Reenable interrupts if they were on
	SREG = sreg;
}

uint16_t flash_get_pagesize()
{
	return SPM_PAGESIZE;
}

/**
 * converts ascii hex to numbers
 */
uint16_t flash_ascii_to_num(const uint8_t * ascii, uint8_t num)
{
	uint8_t i;
	uint16_t val = 0;

	for (i = 0; i < num; ++i)
	{
		uint8_t c = ascii[i];

		if (c >= '0' && c <= '9')
		{
			c -= '0';
		}
		else if (c >= 'A' && c <= 'F')
		{
			c -= 'A' - 10;
		}
		else if (c >= 'a' && c <= 'f')
		{
			c -= 'a' - 10;
		}

		// TODO: Replace by bitshift?
		val = 16 * val + c;
	}

	return val;
}

