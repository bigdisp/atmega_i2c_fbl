/*
 * Host build of the FBL hex parser for testing.
 * Reads a hex file from stdin, prints the result and writes the resulting
 * flash image (8 KiB, erased = 0xFF) plus the list of written pages to stdout.
 */
#include <stdio.h>
#include <string.h>
#include "../src/hex_parser.h"

static uint8_t flash[0x2000];
static int writes;

void flash_write_page(uint16_t addr, const uint8_t *content)
{
	if (addr % SPM_PAGESIZE || addr + SPM_PAGESIZE > FBL_START_ADDR)
	{
		printf("BADPAGE %04x\n", addr);
		return;
	}
	memcpy(&flash[addr], content, SPM_PAGESIZE);
	printf("PAGE %04x\n", addr);
	++writes;
}

int main(void)
{
	int c;
	enum hex_parser_result r = e_hex_busy;

	memset(flash, 0xFF, sizeof(flash));
	hex_parser_init();
	while (e_hex_busy == r && EOF != (c = getchar()))
	{
		r = hex_parser_put((uint8_t) c);
	}
	printf("RESULT %s\n", e_hex_finished == r ? "finished" : e_hex_error == r ? "error" : "busy");
	printf("IMAGE ");
	for (unsigned i = 0; i < sizeof(flash); ++i)
	{
		printf("%02x", flash[i]);
	}
	printf("\n");
	return 0;
}
