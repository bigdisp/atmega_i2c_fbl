/*
 * main.c
 *
 *  Created on: 26.02.2017
 *      Author: martin
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <avr/wdt.h>

#include "lib/i2c.h"
#include "isr_manipulator.h"
#include "hex_parser.h"

#define I2C_DATA_SIZE 4

/// Time the FBL waits for the flash command ('w' 'f') after reset before the
/// application is started (Timer1, F_CPU / 1024 ticks). 1 s @ 8 MHz.
#define FBL_TIMEOUT_TICKS ((uint16_t) (F_CPU / 1024UL))

#ifndef MCUSR
#define MCUSR MCUCSR // atmega8
#endif
#ifndef TIFR
#define TIFR TIFR1 // atmega88
#endif

/**
 * Returns true if something has been flashed to the application area.
 */
static uint8_t app_present(void)
{
	return 0xFFFF != pgm_read_word(0);
}

/**
 * Flash mode.
 *
 * Protocol (lock step): the master writes a chunk of 1 to I2C_BUFFER_SIZE - 2
 * hex file characters, then reads a single byte. The read completes the
 * exchange; only then is the chunk parsed. The byte read back is the last
 * character of the previously processed chunk and can be used for checking.
 *
 * While a chunk is being parsed (and a flash page possibly written), the slave
 * does not acknowledge its address, so the master simply has to retry its write
 * until it is ACKed. No fixed delays are required.
 *
 * @return e_hex_finished or e_hex_error
 */
static enum hex_parser_result run_flasher(void)
{
	enum hex_parser_result result;
	uint8_t rx_buf[I2C_BUFFER_SIZE];
	uint8_t echo = 0;

	hex_parser_init();

	for (;;)
	{
		// Wait until the master has finished its write + read exchange
		i2c_wait_busy();

		u_i2c_status st = i2c_get_status_register();
		if (st.rx_data_in_buf && st.last_rx_ok)
		{
			uint8_t len = i2c_get_rx_len();
			i2c_get_data(rx_buf, len);

			for (uint8_t i = 0; i < len; ++i)
			{
				echo = rx_buf[i];
				result = hex_parser_put(echo);
				if (e_hex_busy != result)
				{
					return result;
				}
				PORTB ^= (1 << PB1);
			}
		}

		// Re-enable the interface (and ACKs) for the next chunk
		i2c_slave_start_tx_data(&echo, 1);
	}
}

/**
 * Restore the reset state of everything the FBL used and jump to the application.
 */
static void __attribute__((noreturn)) start_app(void)
{
	void (*app)(void) = 0x0000;

	cli();
	TWCR = 0;
	TWAR = 0;
	TCCR1B = 0;
	TCNT1 = 0;
	TIFR = 0xFF;
	DDRB = 0;
	PORTB = 0;
	isr_app();
	app();
	for (;;);
}

int main()
{
	// The watchdog stays enabled after a watchdog reset on newer devices
	// (atmega88). The app may use a watchdog reset to enter the FBL.
	MCUSR = 0;
	wdt_disable();

	isr_fbl();

	PORTB &= ~((1 << PB0) | (1 << PB1));
	DDRB |= (1 << PB0) | (1 << PB1);

	// Startup timeout
	TCNT1 = 0;
	TCCR1B = (1 << CS12) | (1 << CS10); // F_CPU / 1024
	// Cleared after a failed flash attempt: the app is incomplete then
	uint8_t timeout_active = 1;
	// 'wf' received, waiting for the hex file
	uint8_t flash_mode = 0;

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
	uint8_t i2c_tx_msg[I2C_DATA_SIZE] = {'F', 'B', 'L', 0xFF}; // Default Start Value: FBL
	uint8_t i2c_rx_msg[I2C_DATA_SIZE];
	u_i2c_status i2c_st_reg;
	i2c_slave_start_tx_data(i2c_tx_msg, 3);

	sei();

	for (;;)
	{
		// Start the application after the timeout unless flashing is in progress / failed
		if (timeout_active && TCNT1 >= FBL_TIMEOUT_TICKS && app_present())
		{
			start_app();
		}

		// Re-enable i2c tx/rx
		i2c_st_reg = i2c_get_status_register();
		if (i2c_busy())
		{
			// Interface is enabled (possibly mid-transfer). Don't touch it, re-arming
			// it here would clear rx_data_in_buf and could drop received data.
		}
		else if (i2c_st_reg.tx_data_in_buf && !i2c_st_reg.rx_data_in_buf)
		{
			// There is still tx data in the buffer, simply re-enable rx/tx
			i2c_slave_start_rx();
		}
		else if (!i2c_st_reg.tx_data_in_buf)
		{
			// Data was sent, send more
			i2c_slave_start_tx_data(i2c_tx_msg, I2C_DATA_SIZE);
		}

		i2c_st_reg = i2c_get_status_register();
		if (!(i2c_st_reg.rx_data_in_buf && i2c_st_reg.last_rx_ok))
		{
			continue;
		}

		if (flash_mode)
		{
			// First chunk of the hex file received, the flasher fetches it itself
			flash_mode = 0;
			if (e_hex_finished == run_flasher())
			{
				// Done, a new application is in place
				start_app();
			}

			// Error: stay in the FBL (the application is probably incomplete) and
			// report the error. The master has to restart flashing with 'wf'.
			PORTB |= (1 << PB0);
			i2c_tx_msg[0] = 'E';
			i2c_tx_msg[1] = 'R';
			i2c_tx_msg[2] = 'R';
			i2c_tx_msg[3] = 0;
			i2c_slave_start_tx_data(i2c_tx_msg, I2C_DATA_SIZE);
			timeout_active = 0;
			continue;
		}

		i2c_get_data(i2c_rx_msg, I2C_DATA_SIZE);

		// Data sent: 0x77 0x66
		if ('w' == i2c_rx_msg[0] && 'f' == i2c_rx_msg[1])
		{
			// Switch to flash mode. The master writes the hex file next.
			// If it doesn't do so within the timeout, the app is started anyway.
			flash_mode = 1;
			TCNT1 = 0;
			i2c_tx_msg[0] = 'f'; // 0x66
			i2c_tx_msg[1] = 's'; // 0x73
			i2c_tx_msg[2] = 0;
			i2c_tx_msg[3] = 0;
			i2c_slave_start_tx_data(i2c_tx_msg, I2C_DATA_SIZE);
		}
		// Data sent: 0x73 or 0x53
		else if (('s' == i2c_rx_msg[0] || 'S' == i2c_rx_msg[0]) && app_present())
		{
			start_app();
		}
		else
		{
			// Unknown command: echo it. This does not stop the timeout, so a master
			// talking to the (not yet started) app can't keep the system in the FBL.
			for (uint8_t i = 0; i < I2C_DATA_SIZE; ++i)
			{
				i2c_tx_msg[i] = i2c_rx_msg[i];
			}
		}
	}

	return 0;
}
