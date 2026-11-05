/*
 * Host mock of <avr/boot.h>: self-programming (SPM) on a simulated 8 KiB
 * atmega8 flash with 64-byte pages. Implemented in tests/mock/fbl_mock.c.
 *
 * Like the real hardware, page_fill only writes a temporary page buffer and
 * page_write copies it to the (previously erased) flash page.
 */
#ifndef MOCK_AVR_BOOT_H
#define MOCK_AVR_BOOT_H

#include <stdint.h>
#include <avr/eeprom.h>

#ifndef SPM_PAGESIZE
#define SPM_PAGESIZE 64
#endif
#define MOCK_FLASH_SIZE 8192

extern uint8_t mock_flash[MOCK_FLASH_SIZE];

void mock_boot_page_erase(uint32_t addr);
void mock_boot_page_fill(uint32_t addr, uint16_t word);
void mock_boot_page_write(uint32_t addr);
void mock_boot_rww_enable(void);

#define boot_page_erase(addr)       mock_boot_page_erase(addr)
#define boot_page_fill(addr, word)  mock_boot_page_fill(addr, word)
#define boot_page_write(addr)       mock_boot_page_write(addr)
#define boot_rww_enable()           mock_boot_rww_enable()
#define boot_spm_busy_wait()        ((void) 0)

#endif
