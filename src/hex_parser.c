/*
 * hex_parser.c
 *
 * Streaming Intel HEX parser, see hex_parser.h.
 */

#include <string.h>
#include "hex_parser.h"

// Binary record: length, address high, address low, type, data..., checksum
static uint8_t rec[HEX_MAX_DATA + 5];
static uint8_t rec_cnt;
static uint8_t in_record;
static uint8_t have_nibble;
static uint8_t nibble;
static uint8_t sum;

// Page buffer
static uint8_t page[SPM_PAGESIZE];
static uint16_t page_addr;
static uint8_t page_used;

// Page 0 (reset vector) is written last, so an interrupted update never
// leaves something behind that looks like a valid application.
static uint8_t page0[SPM_PAGESIZE];

static void page_flush(void)
{
	if (page_used)
	{
		if (0 == page_addr)
		{
			memcpy(page0, page, sizeof(page0));
		}
		else
		{
			flash_write_page(page_addr, page);
		}
		memset(page, 0xFF, sizeof(page));
		page_used = 0;
	}
}

void hex_parser_init(void)
{
	in_record = 0;
	page_used = 0;
	page_addr = 0;
	memset(page, 0xFF, sizeof(page));
	memset(page0, 0xFF, sizeof(page0));

	// Invalidate the current application until the new one is complete
	flash_write_page(0, page);
}

static enum hex_parser_result record_done(void)
{
	uint8_t len = rec[0];
	uint16_t addr = ((uint16_t) rec[1] << 8) | rec[2];

	if (sum)
	{
		// Checksum error
		return e_hex_error;
	}

	switch (rec[3])
	{
	case 0x00: // Data
		for (uint8_t i = 0; i < len; ++i, ++addr)
		{
			if (addr >= FBL_START_ADDR)
			{
				// Never overwrite the FBL
				return e_hex_error;
			}

			uint16_t base = addr & ~(uint16_t)(SPM_PAGESIZE - 1);
			if (base != page_addr)
			{
				if (base < page_addr)
				{
					// Going back would erase an already written page
					return e_hex_error;
				}
				page_flush();
				page_addr = base;
			}
			page[addr & (SPM_PAGESIZE - 1)] = rec[4 + i];
			page_used = 1;
		}
		return e_hex_busy;

	case 0x01: // End of file
		page_flush();
		flash_write_page(0, page0);
		return e_hex_finished;

	case 0x02: // Extended segment address
	case 0x04: // Extended linear address
		// Only the first 64 KiB are supported
		if (2 != len || rec[4] || rec[5])
		{
			return e_hex_error;
		}
		return e_hex_busy;

	default: // Start address records (03, 05): not needed
		return e_hex_busy;
	}
}

enum hex_parser_result hex_parser_put(uint8_t c)
{
	uint8_t v;

	if (':' == c)
	{
		if (in_record)
		{
			// Previous record is incomplete
			return e_hex_error;
		}
		in_record = 1;
		rec_cnt = 0;
		have_nibble = 0;
		sum = 0;
		return e_hex_busy;
	}

	if (!in_record)
	{
		// Line breaks etc. between records
		return e_hex_busy;
	}

	if (c >= '0' && c <= '9')
	{
		v = c - '0';
	}
	else
	{
		c |= 0x20; // lower case
		if (c >= 'a' && c <= 'f')
		{
			v = c - ('a' - 10);
		}
		else
		{
			return e_hex_error;
		}
	}

	if (!have_nibble)
	{
		nibble = v << 4;
		have_nibble = 1;
		return e_hex_busy;
	}
	have_nibble = 0;
	v |= nibble;

	rec[rec_cnt++] = v;
	sum += v;

	if (1 == rec_cnt && v > HEX_MAX_DATA)
	{
		return e_hex_error;
	}

	if (rec_cnt == rec[0] + 5)
	{
		in_record = 0;
		return record_done();
	}

	return e_hex_busy;
}
