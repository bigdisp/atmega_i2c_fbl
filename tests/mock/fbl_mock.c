/*
 * Host mocks for the flash bootloader: AVR registers, SPM flash programming
 * and EEPROM.
 */
#include "fbl_mock.h"
#include <string.h>

volatile uint8_t PORTB, DDRB, GICR, SREG;
int mock_irq_enabled;
uint8_t mock_eeprom[MOCK_EEPROM_SIZE];
uint8_t mock_flash[MOCK_FLASH_SIZE];

struct mock_spm mock_spm;

static uint8_t page_buffer[SPM_PAGESIZE];
static uint8_t page_erased[MOCK_FLASH_SIZE / SPM_PAGESIZE];

void mock_reset(void)
{
	PORTB = DDRB = GICR = SREG = 0;
	mock_irq_enabled = 0;
	memset(mock_eeprom, 0xFF, sizeof(mock_eeprom));
	memset(mock_flash, 0xFF, sizeof(mock_flash));
	memset(page_buffer, 0xFF, sizeof(page_buffer));
	memset(page_erased, 0, sizeof(page_erased));
	memset(&mock_spm, 0, sizeof(mock_spm));
}

/* ---- SPM ------------------------------------------------------------- */

void mock_boot_page_erase(uint32_t addr)
{
	if (addr % SPM_PAGESIZE) mock_spm.unaligned++;
	if (mock_irq_enabled) mock_spm.irq_enabled_during_spm++;
	addr %= MOCK_FLASH_SIZE;
	addr -= addr % SPM_PAGESIZE;
	memset(&mock_flash[addr], 0xFF, SPM_PAGESIZE);
	page_erased[addr / SPM_PAGESIZE] = 1;
	mock_spm.erase_count++;
}

void mock_boot_page_fill(uint32_t addr, uint16_t word)
{
	uint32_t off = addr % SPM_PAGESIZE;
	page_buffer[off & ~1u]       = (uint8_t) (word & 0xFF);
	page_buffer[(off & ~1u) + 1] = (uint8_t) (word >> 8);
	mock_spm.fill_count++;
}

void mock_boot_page_write(uint32_t addr)
{
	uint32_t i;
	if (addr % SPM_PAGESIZE) mock_spm.unaligned++;
	addr %= MOCK_FLASH_SIZE;
	addr -= addr % SPM_PAGESIZE;
	if (!page_erased[addr / SPM_PAGESIZE]) mock_spm.write_without_erase++;
	for (i = 0; i < SPM_PAGESIZE; ++i)
	{
		mock_flash[addr + i] &= page_buffer[i]; /* flash can only clear bits */
	}
	page_erased[addr / SPM_PAGESIZE] = 0;
	memset(page_buffer, 0xFF, sizeof(page_buffer));
	if (mock_spm.write_count < 256) mock_spm.written_pages[mock_spm.write_count] = addr;
	mock_spm.write_count++;
}

void mock_boot_rww_enable(void) {}

/* ---- EEPROM ---------------------------------------------------------- */

uint8_t eeprom_read_byte(const uint8_t *addr)
{
	return mock_eeprom[(uintptr_t) addr % MOCK_EEPROM_SIZE];
}

