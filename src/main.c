/*
 * main.c
 *
 *  Created on: 26.02.2017
 *      Author: martin
 */

#include<avr/io.h>

#include "lib/i2c.h"
#include "isr_manipulator.h"
#include "flash.h"
#include <avr/wdt.h>
#include <string.h>
#include <avr/interrupt.h>

#define I2C_DATA_SIZE 4
#define APP_START_ADDR 0x0000

enum fbl_state
{
	e_fbl_state_wait,
	e_fbl_state_parse,
	e_fbl_state_start_app
};


enum flash_parser_state flash_state_machine(enum flash_parser_state state, uint8_t rx_char)
{
	// Checksum of received data
	static uint16_t hex_check = 0;
	// target address (hex)
	static uint16_t hex_addr = 0;
	// target flash page to be written
	static uint16_t flash_page = 0;
	// write pointer for flash buffer (converted data)
	static uint16_t flash_cnt = 0;


	// Symbol counter for write position
	static uint8_t hex_cnt = 0;
	// Buffer for ascii data conversion
	static uint8_t hex_buffer[5];
	// Buffer for converted data (to be written)
	static uint8_t flash_data[SPM_PAGESIZE];
	// Size of the received hex data in one line
	static uint8_t hex_size = 0;
	// Receive counter for received hex data in one line
	static uint8_t hex_data_cnt = 0;
	// Received hex type
	static enum flash_parser_line_type hex_type = e_parser_line_type_data;
	// Received checksum (ASCII)
	static uint8_t hex_checksum = 0;

	// Flag to determine start address of page for writing
	static uint8_t flash_page_flag = 1;


	switch(state)
	{
		//Initialization
	case e_parser_state_start:
		if (FLASH_PARSER_SYM_LINE_START == rx_char)
		{
			state = e_parser_state_size;
			hex_cnt = 0;
			hex_check = 0;
		}
		break;
		// Parse size of data
	case e_parser_state_size:
		hex_buffer[hex_cnt] = rx_char;
		++hex_cnt;
		if (2 == hex_cnt)
		{
			state = e_parser_state_address;
			hex_cnt = 0;
			hex_size = (uint8_t) flash_ascii_to_num(hex_buffer, 2);
			hex_check += hex_size;
		}
		break;
		// Parse address to write to
	case e_parser_state_address:
		hex_buffer[hex_cnt] = rx_char;
		++ hex_cnt;
		if (4 == hex_cnt)
		{
			state = e_parser_state_type;
			hex_cnt = 0;
			hex_addr = flash_ascii_to_num(hex_buffer, 4);
			hex_check += (uint8_t) hex_addr;
			hex_check += (uint8_t) (hex_addr >> 8);
			if (flash_page_flag)
			{
				flash_page = hex_addr - (hex_addr % SPM_PAGESIZE);
				flash_page_flag = 0;
			}
		}
		break;
		// Parse type of line (data / checksum)
	case e_parser_state_type:
		hex_buffer[hex_cnt] = rx_char;
		++hex_cnt;

		if (2 == hex_cnt)
		{
			hex_cnt = 0;
			hex_data_cnt = 0;
			hex_type = (uint8_t) flash_ascii_to_num(hex_buffer, 2);
			hex_check += hex_type;
			// Different datatypes require different followup handling
			switch (hex_type)
			{
			case e_parser_line_type_checksum:
				state = e_parser_state_checksum;
				break;
			case e_parser_line_type_data:
			default:
				state = e_parser_state_data;
				break;
			}
		}
		break;
		// read data
	case e_parser_state_data_written:
	case e_parser_state_data:
		hex_buffer[hex_cnt] = rx_char;
		++hex_cnt;

		if (2 == hex_cnt)
		{
			hex_cnt = 0;
			flash_data[flash_cnt] = (uint8_t) flash_ascii_to_num(hex_buffer, 2);
			hex_check += flash_data[flash_cnt];
			++flash_cnt;
			++hex_data_cnt;

			if (hex_size == hex_data_cnt)
			{
				state = e_parser_state_checksum;
				hex_data_cnt = 0;
			}

			// Buffer is full. We need to write
			// TODO: While data received is periodically checked, the last part will not be checked.
			// This needs to be moved after the checksum parsing to ensure we only write checked data.
			if (SPM_PAGESIZE == flash_cnt)
			{
				flash_write_page(flash_page, flash_data);
				memset(flash_data, 0xFF, sizeof(flash_data));
				flash_cnt = 0;
				flash_page_flag = 1;
				if (e_parser_state_checksum == state)
				{
					state = e_parser_state_checksum_data_written;
				}
				else
				{
					state = e_parser_state_data_written;
				}
			}
		}
		break;
		// Checksum
	case e_parser_state_checksum_data_written:
	case e_parser_state_checksum:
		hex_buffer[hex_cnt] = rx_char;
		++hex_cnt;

		if (2 == hex_cnt)
		{
			hex_checksum = (uint8_t) flash_ascii_to_num(hex_buffer, 2);
			hex_check += hex_checksum;
			hex_check &= 0x00FF;

			if (e_parser_line_type_checksum == hex_type)
			{
				flash_write_page(flash_page, flash_data);
				state = e_parser_state_finished;
			}
			else if (0 == hex_check)
			{
				state = e_parser_state_start;
			}
			else
			{
				state = e_parser_state_error;
			}
		}
		break;
	case e_parser_state_error:
		break;
	default:
		break;
	}
	PORTB ^= (1 << PB1);
	return state;
}

/**
 * Runs the flasher undisturbed of any i2c handling. Don't write the letters too quickly.
 */
void run_flasher(uint8_t rx_char)
{
	enum flash_parser_state parser_state = e_parser_state_start;
	i2c_slave_start_tx_data(&rx_char, 1);
	parser_state = flash_state_machine(parser_state, rx_char);

	while(e_parser_state_finished != parser_state && e_parser_state_error != parser_state)
	{
		i2c_wait_busy();
		if (i2c_get_data(&rx_char, 1))
		{
			parser_state = flash_state_machine(parser_state, rx_char);
		}
		i2c_slave_start_tx_data(&rx_char, 1);
	}
}

int main()
{
	isr_fbl();
	void (*app_start)(void) = APP_START_ADDR;

	// Watchdog:
	//wdt_enable(WDTO_2S);
	//wdt_reset();

	PORTB &= ~(1 << PB0);
	PORTB &= ~(1 << PB1);

	DDRB |= (1 << PB0);
	DDRB |= (1 << PB1);

	//i2c / TWI
	uint8_t slave_addr;
	slave_addr = eeprom_read_byte(0);

	// Check for illegal addresses:
	if (slave_addr >= 0xF0 || slave_addr <= 0x0F)
	{
		// Set the default address in this case:
		slave_addr = 0x14;
	}

	i2c_init(slave_addr);
	//uint8_t i2c_msg[I2C_DATA_SIZE] = {0xFF, 0xFF, 0xFF, 0xFF};
	uint8_t i2c_tx_msg[I2C_DATA_SIZE] = {0x46, 0x42, 0x4C, 0xFF}; // Default Start Value: FBL
	uint8_t i2c_rx_msg[I2C_DATA_SIZE] = {0x46, 0x42, 0x4C, 0xFF};
	u_i2c_status i2c_st_reg;
	i2c_slave_start_tx_data(i2c_tx_msg,3);

	enum fbl_state fbl_state = e_fbl_state_wait;

	sei();

	do
	{
		// Re-enable i2c tx/rx
		i2c_st_reg = i2c_get_status_register();
		if (i2c_st_reg.tx_data_in_buf && !i2c_st_reg.rx_data_in_buf)
		{
			// There is still tx data in the buffer, simply re-enable rx/tx
			i2c_slave_start_rx();
		}
		else if (!i2c_st_reg.tx_data_in_buf)
		{
			// Data was sent, send more
			i2c_slave_start_tx_data(i2c_tx_msg, I2C_DATA_SIZE);
		}

		// TODO: Read symbol from i2c
		i2c_st_reg = i2c_get_status_register();

		if (i2c_st_reg.rx_data_in_buf && i2c_st_reg.last_rx_ok)
		{
			i2c_get_data(i2c_rx_msg, I2C_DATA_SIZE);
		}

		// State machine of FBL
		if (e_fbl_state_parse == fbl_state && i2c_st_reg.rx_data_in_buf && i2c_st_reg.last_rx_ok)
		{
			// Mode: Flash
			//

			run_flasher(i2c_rx_msg[0]);
			fbl_state = e_fbl_state_wait;
		}
		else if (e_fbl_state_wait == fbl_state)
		{
			// Mode: Wait for instructions
			//
			// TODO: Check compatibility of fbl commands with application

			// Data sent: 0x77 0x66
			if ('w' == i2c_rx_msg[0] && 'f' == i2c_rx_msg[1])
			{
				// Switch to flash mode
				fbl_state = e_fbl_state_parse;
				i2c_tx_msg[0] = 'f'; // 0x66
				i2c_tx_msg[1] = 's'; // 0x73
				i2c_tx_msg[2] = 0;
				i2c_tx_msg[3] = 0;
				i2c_slave_start_tx_data(i2c_tx_msg, I2C_DATA_SIZE);

			}
			// Data sent: 0x73 or 0x53
			else if ('s' == i2c_rx_msg[0] || 'S' == i2c_rx_msg[0])
			{
				// Start application
				fbl_state = e_fbl_state_start_app;
				i2c_tx_msg[0] = 's';
				i2c_tx_msg[1] = 'a';
				i2c_tx_msg[2] = 'p';
				i2c_tx_msg[3] = 'p';
				i2c_slave_start_tx_data(i2c_tx_msg, I2C_DATA_SIZE);
			}
			else
			{
				for (unsigned int i = 0; i < I2C_DATA_SIZE; ++i)
				{
					i2c_tx_msg[i] = i2c_rx_msg[i];
				}
			}

			//TODO: Wait for a specified time, then boot app if no signal is received
		}

		// Reset watchdog
		//wdt_reset();

	} while(e_fbl_state_start_app != fbl_state);

	isr_app();
	app_start();

	return 0;
}
