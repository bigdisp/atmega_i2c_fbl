/* Shared declarations of the host mocks, see fbl_mock.c. */
#ifndef FBL_MOCK_H
#define FBL_MOCK_H

#include <stdint.h>
#include <avr/io.h>
#include <avr/boot.h>
#include <avr/interrupt.h>

struct mock_spm
{
	int erase_count;
	int write_count;
	int fill_count;
	int write_without_erase;     ///< page_write on a page not erased before
	int unaligned;               ///< erase/write address not page aligned
	int irq_enabled_during_spm;  ///< SPM while interrupts were enabled
	uint32_t written_pages[256]; ///< page addresses in write order
};

extern struct mock_spm mock_spm;

void mock_reset(void);

#endif
