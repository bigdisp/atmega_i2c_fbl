/*
 * PC test suite for the I2C flash bootloader.
 *
 * src/hex_parser.c and src/flash.c are compiled unchanged for the host.
 * Flash self-programming (SPM) is simulated on an 8 KiB atmega8 flash image
 * (tests/mock), so the complete path "Intel HEX text -> parser -> SPM page
 * programming" is exercised without a microcontroller.
 *
 * Complements tests/test_hex_parser.py (parser only, Python driven).
 */
#include <stdlib.h>
#include <string.h>
#include "mock/fbl_mock.h"
#include "flash.h"
#include "hex_parser.h"
#include "minitest.h"

/* ---- Intel HEX helpers ---------------------------------------------- */

static char *hex_append_record(char *out, uint8_t type, uint16_t addr, const uint8_t *data, int len, int corrupt)
{
	uint8_t sum = (uint8_t) (len + (addr >> 8) + (addr & 0xFF) + type);
	int i;
	out += sprintf(out, ":%02X%04X%02X", len, addr, type);
	for (i = 0; i < len; ++i)
	{
		out += sprintf(out, "%02X", data[i]);
		sum += data[i];
	}
	out += sprintf(out, "%02X\n", (uint8_t) (-sum + corrupt));
	return out;
}

/* HEX file for len bytes at base, records of line_len bytes, optional corrupt record. */
static char *make_hex(const uint8_t *data, int len, uint16_t base, int line_len, int corrupt_line)
{
	char *buf = calloc(1, (size_t) len * 3 + 64 * (len / line_len + 2));
	char *p = buf;
	int off, line = 0;
	for (off = 0; off < len; off += line_len, ++line)
	{
		int n = len - off < line_len ? len - off : line_len;
		p = hex_append_record(p, 0x00, (uint16_t) (base + off), data + off, n, line == corrupt_line);
	}
	hex_append_record(p, 0x01, 0, NULL, 0, 0);
	return buf;
}

static void fill_random(uint8_t *data, int len, unsigned seed)
{
	int i;
	srand(seed);
	for (i = 0; i < len; ++i)
	{
		data[i] = (uint8_t) rand();
	}
}

/* Start a new session and feed a complete string. */
static enum hex_parser_result feed(const char *s)
{
	enum hex_parser_result r = e_hex_busy;
	hex_parser_init();
	for (; *s && e_hex_busy == r; ++s)
	{
		r = hex_parser_put((uint8_t) *s);
	}
	return r;
}

static int flash_equals(uint16_t addr, const uint8_t *data, int len)
{
	return memcmp(&mock_flash[addr], data, (size_t) len) == 0;
}

static int flash_is_erased(uint16_t addr, int len)
{
	int i;
	for (i = 0; i < len; ++i)
	{
		if (mock_flash[addr + i] != 0xFF) return 0;
	}
	return 1;
}

/* Flash len random bytes at base and compare the WHOLE flash with the expected image. */
static void flash_and_verify(int len, uint16_t base, int line_len, unsigned seed)
{
	uint8_t *data = malloc((size_t) len);
	uint8_t *expected = malloc(MOCK_FLASH_SIZE);
	char *hex;
	mock_reset();
	mock_irq_enabled = 1;
	fill_random(data, len, seed);
	memset(expected, 0xFF, MOCK_FLASH_SIZE);
	memcpy(&expected[base], data, (size_t) len);
	hex = make_hex(data, len, base, line_len, -1);

	CHECK_EQ(e_hex_finished, feed(hex));
	CHECK(memcmp(mock_flash, expected, MOCK_FLASH_SIZE) == 0);
	CHECK_EQ(0, mock_spm.unaligned);
	CHECK_EQ(0, mock_spm.write_without_erase);
	CHECK_EQ(0, mock_spm.irq_enabled_during_spm);
	CHECK(mock_spm.write_count >= 2);
	CHECK_EQ(0, mock_spm.written_pages[0]);                     /* app invalidated first */
	CHECK_EQ(0, mock_spm.written_pages[mock_spm.write_count - 1]); /* reset vector written last */
	free(hex);
	free(expected);
	free(data);
}

/* ======================================================================= */
/* flash.c on simulated SPM                                                */
/* ======================================================================= */

TEST_CASE(write_page_programs_whole_page)
{
	uint8_t page[SPM_PAGESIZE];
	mock_reset();
	fill_random(page, SPM_PAGESIZE, 1);
	mock_irq_enabled = 1;
	SREG = 0x80;
	flash_write_page(0x0140, page);
	CHECK(flash_equals(0x0140, page, SPM_PAGESIZE));
	CHECK_EQ(1, mock_spm.erase_count);
	CHECK_EQ(1, mock_spm.write_count);
	CHECK_EQ(SPM_PAGESIZE / 2, mock_spm.fill_count);
	CHECK_EQ(0, mock_spm.irq_enabled_during_spm);
	CHECK_EQ(0x80, SREG); /* interrupt state restored */
	CHECK(flash_is_erased(0x0100, 0x40));
	CHECK(flash_is_erased(0x0180, 0x40));
}

TEST_CASE(write_page_overwrites_previous_content)
{
	uint8_t a[SPM_PAGESIZE], b[SPM_PAGESIZE];
	mock_reset();
	fill_random(a, SPM_PAGESIZE, 2);
	fill_random(b, SPM_PAGESIZE, 3);
	flash_write_page(0x0400, a);
	flash_write_page(0x0400, b);
	CHECK(flash_equals(0x0400, b, SPM_PAGESIZE));
	CHECK_EQ(0, mock_spm.write_without_erase);
}

/* ======================================================================= */
/* Intel HEX parser                                                        */
/* ======================================================================= */

TEST_CASE(init_invalidates_application)
{
	mock_reset();
	memset(mock_flash, 0x12, 0x100);
	hex_parser_init();
	CHECK(flash_is_erased(0, SPM_PAGESIZE));
	CHECK_EQ(0x12, mock_flash[SPM_PAGESIZE]);
}

TEST_CASE(characters_outside_records_are_ignored)
{
	mock_reset();
	CHECK_EQ(e_hex_busy, feed("\r\n \t xyz\n"));
}

TEST_CASE(bad_checksum_is_an_error)
{
	uint8_t data[16];
	char *hex;
	mock_reset();
	fill_random(data, sizeof(data), 4);
	hex = make_hex(data, sizeof(data), 0x100, 16, 0);
	CHECK_EQ(e_hex_error, feed(hex));
	free(hex);
}

TEST_CASE(malformed_records_are_errors)
{
	mock_reset();
	CHECK_EQ(e_hex_error, feed(":10000000G0"));     /* non-hex digit */
	CHECK_EQ(e_hex_error, feed(":100000\n"));       /* line break inside record */
	CHECK_EQ(e_hex_error, feed(":0100:"));          /* new record before old one ended */
	CHECK_EQ(e_hex_error, feed(":21000000"));       /* 33 data bytes > HEX_MAX_DATA */
	CHECK_EQ(e_hex_error, feed(":020000040001F9")); /* extended linear address != 0 */
}

TEST_CASE(extended_address_zero_is_accepted)
{
	mock_reset();
	CHECK_EQ(e_hex_finished, feed(":020000040000FA\n:00000001FF\n"));
}

TEST_CASE(flash_image_16_byte_records)       { flash_and_verify(200, 0x0000, 16, 5); }
TEST_CASE(flash_image_32_byte_records)       { flash_and_verify(1000, 0x0000, 32, 6); }
TEST_CASE(flash_image_records_crossing_pages) { flash_and_verify(703, 0x0000, 19, 7); }
TEST_CASE(flash_image_at_page_aligned_offset) { flash_and_verify(300, 0x0400, 16, 8); }
TEST_CASE(flash_image_at_unaligned_offset)   { flash_and_verify(300, 0x0123, 16, 9); }
TEST_CASE(flash_image_fills_app_section)     { flash_and_verify(FBL_START_ADDR, 0x0000, 16, 10); }

TEST_CASE(lowercase_hex_and_crlf_are_accepted)
{
	uint8_t data[100];
	char *hex, *p, *crlf, *q;
	mock_reset();
	fill_random(data, sizeof(data), 11);
	hex = make_hex(data, sizeof(data), 0, 16, -1);
	crlf = calloc(1, strlen(hex) * 2 + 1);
	for (p = hex, q = crlf; *p; ++p)
	{
		if (*p == '\n') *q++ = '\r';
		*q++ = (*p >= 'A' && *p <= 'F') ? (char) (*p - 'A' + 'a') : *p;
	}
	CHECK_EQ(e_hex_finished, feed(crlf));
	CHECK(flash_equals(0, data, sizeof(data)));
	free(hex);
	free(crlf);
}

TEST_CASE(fbl_section_is_never_written)
{
	uint8_t data[32];
	char *hex;
	mock_reset();
	fill_random(data, sizeof(data), 12);
	hex = make_hex(data, sizeof(data), FBL_START_ADDR - 16, 16, -1);
	CHECK_EQ(e_hex_error, feed(hex));
	CHECK(flash_is_erased(FBL_START_ADDR - SPM_PAGESIZE, MOCK_FLASH_SIZE - FBL_START_ADDR + SPM_PAGESIZE));
	free(hex);
}

TEST_CASE(descending_addresses_are_rejected)
{
	uint8_t a[16];
	char hex[256], *p = hex;
	mock_reset();
	fill_random(a, 16, 13);
	p = hex_append_record(p, 0, 0x0200, a, 16, 0);
	p = hex_append_record(p, 0, 0x0100, a, 16, 0);
	hex_append_record(p, 1, 0, NULL, 0, 0);
	CHECK_EQ(e_hex_error, feed(hex));
}

TEST_CASE(interrupted_transfer_leaves_no_valid_app)
{
	uint8_t data[512];
	char *hex;
	mock_reset();
	memset(mock_flash, 0x00, FBL_START_ADDR); /* old application */
	fill_random(data, sizeof(data), 14);
	hex = make_hex(data, sizeof(data), 0, 16, -1);
	hex[strlen(hex) / 2] = '\0'; /* transfer stops half way */
	CHECK_EQ(e_hex_busy, feed(hex));
	CHECK(flash_is_erased(0, SPM_PAGESIZE)); /* reset vector erased -> FBL stays active */
	free(hex);
}

/* ---- Regression tests for bugs of the original parser (fixed) -------- */

/* EOF record used to re-arm the page pointer to 0x0000 -> last page landed in page 0 */
TEST_CASE(image_with_full_last_page_keeps_page_0)       { flash_and_verify(256, 0x0000, 16, 15); }
TEST_CASE(last_record_crossing_page_lands_in_right_page) { flash_and_verify(777, 0x0000, 19, 16); }

/* Page buffer used to be zero-initialised -> unused bytes programmed as 0x00 */
TEST_CASE(small_image_leaves_rest_of_page_erased)       { flash_and_verify(20, 0x0000, 16, 17); }

/* Record addresses used to be ignored within a page */
TEST_CASE(address_gaps_are_respected)
{
	uint8_t a[16], b[16];
	char hex[512], *p = hex;
	mock_reset();
	fill_random(a, 16, 18);
	fill_random(b, 16, 19);
	p = hex_append_record(p, 0, 0x0000, a, 16, 0);
	p = hex_append_record(p, 0, 0x0020, b, 16, 0);
	p = hex_append_record(p, 0, 0x0300, b, 16, 0);
	hex_append_record(p, 1, 0, NULL, 0, 0);
	CHECK_EQ(e_hex_finished, feed(hex));
	CHECK(flash_equals(0x0000, a, 16));
	CHECK(flash_is_erased(0x0010, 16));
	CHECK(flash_equals(0x0020, b, 16));
	CHECK(flash_is_erased(0x0030, 0x0300 - 0x0030));
	CHECK(flash_equals(0x0300, b, 16));
}

/* Pages used to be programmed before the checksum of their last record was checked */
TEST_CASE(corrupt_record_is_not_programmed)
{
	uint8_t data[64];
	char *hex;
	mock_reset();
	fill_random(data, sizeof(data), 20);
	hex = make_hex(data, sizeof(data), 0x0100, 16, 3); /* last record of the page corrupt */
	CHECK_EQ(e_hex_error, feed(hex));
	CHECK(flash_is_erased(0x0100, SPM_PAGESIZE));
	free(hex);
}

/* Parser state used to survive between sessions */
TEST_CASE(second_session_after_error_starts_clean)
{
	uint8_t data[100];
	char *bad, *good;
	mock_reset();
	fill_random(data, sizeof(data), 21);
	bad = make_hex(data, 40, 0, 16, 1);
	good = make_hex(data, sizeof(data), 0, 16, -1);
	CHECK_EQ(e_hex_error, feed(bad));
	CHECK_EQ(e_hex_finished, feed(good));
	CHECK(flash_equals(0, data, sizeof(data)));
	CHECK(flash_is_erased(sizeof(data), FBL_START_ADDR - (int) sizeof(data)));
	free(bad);
	free(good);
}

TEST_CASE(second_session_after_abort_mid_record_starts_clean)
{
	uint8_t data[100];
	char *good;
	mock_reset();
	fill_random(data, sizeof(data), 22);
	good = make_hex(data, sizeof(data), 0, 16, -1);
	CHECK_EQ(e_hex_busy, feed(":1000000001020304")); /* master died mid record */
	CHECK_EQ(e_hex_finished, feed(good));
	CHECK(flash_equals(0, data, sizeof(data)));
	free(good);
}

int main(void)
{
	printf("== flash.c (simulated SPM)\n");
	RUN(write_page_programs_whole_page);
	RUN(write_page_overwrites_previous_content);

	printf("== Intel HEX parser -> flash\n");
	RUN(init_invalidates_application);
	RUN(characters_outside_records_are_ignored);
	RUN(bad_checksum_is_an_error);
	RUN(malformed_records_are_errors);
	RUN(extended_address_zero_is_accepted);
	RUN(flash_image_16_byte_records);
	RUN(flash_image_32_byte_records);
	RUN(flash_image_records_crossing_pages);
	RUN(flash_image_at_page_aligned_offset);
	RUN(flash_image_at_unaligned_offset);
	RUN(flash_image_fills_app_section);
	RUN(lowercase_hex_and_crlf_are_accepted);
	RUN(fbl_section_is_never_written);
	RUN(descending_addresses_are_rejected);
	RUN(interrupted_transfer_leaves_no_valid_app);

	printf("== regressions (bugs of the original parser)\n");
	RUN(image_with_full_last_page_keeps_page_0);
	RUN(last_record_crossing_page_lands_in_right_page);
	RUN(small_image_leaves_rest_of_page_erased);
	RUN(address_gaps_are_respected);
	RUN(corrupt_record_is_not_programmed);
	RUN(second_session_after_error_starts_clean);
	RUN(second_session_after_abort_mid_record_starts_clean);

	return minitest_summary();
}
