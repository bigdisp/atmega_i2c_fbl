/*
 * flash.c
 *
 *  Created on: 26.02.2017
 *      Author: martin
 */

#include "flash.h"
#include <avr/io.h>
#include <avr/boot.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>


void flash_write_page(uint16_t addr, const uint8_t *content)
{
	uint8_t sreg;
	uint8_t i;

	// Interrupt disable
	sreg = SREG;
	cli();

	// SPM must not be used while the EEPROM is being written
	eeprom_busy_wait();

	boot_page_erase(addr);
	boot_spm_busy_wait();

	// Fill the write buffer
	// Data is written 2 byte at a time, but addresses are on bytes
	// So we need to increment by two on each loop
	for (i = 0; i < SPM_PAGESIZE; i += 2)
	{
		uint16_t w = content[i] | ((uint16_t) content[i + 1] << 8);
		boot_page_fill(addr + i, w);
	}
	boot_page_write(addr);
	boot_spm_busy_wait();

	boot_rww_enable();

	// Reenable interrupts if they were on
	SREG = sreg;
}
