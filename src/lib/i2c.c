/*
 * i2c.c
 *
 *  Created on: 10.10.2013
 *  Author: Martin Beckmann
 */
//#define DEBUG
#include<util/twi.h>
#include<avr/io.h>
#include "i2c.h"

uint8_t i2c_rx_buf[I2C_BUFFER_SIZE];
uint8_t i2c_tx_buf[I2C_BUFFER_SIZE];
uint8_t i2c_msg_size = 0;
uint8_t i2c_status = I2C_NO_STATE;
u_i2c_status i2c_status_register = {0};

// From i2c_interrupt.c:
extern uint8_t i2c_tx_buf_ptr;

void i2c_init(uint8_t address)
{
	TWAR = (address << 1);
	TWDR = 0xFF;
	TWCR =  (1 << TWEN);  // Enable TWI, don't answer to any calls yet

	//Empty buffer:
	for(uint8_t i = 0; i < I2C_BUFFER_SIZE; i++)
	{
		i2c_tx_buf[i] = 0;
		i2c_rx_buf[i] = 0;
	}
	i2c_status = 0;
}

uint8_t i2c_get_status()
{
	return i2c_status;
}

u_i2c_status i2c_get_status_register()
{
	return i2c_status_register;
}

/**
 * Check whether the Interface is currently enabled to send/receive.
 */
uint8_t i2c_busy()
{
	return TWCR & (1 << TWIE); //When Interrupt is enabled, the i2c Bus is busy
}

/**
 * Checks whether the interface is currently actively transmitting/receiving.
 */
uint8_t i2c_busy_int()
{
	return (TWCR & (1 << TWINT));
}

/**
 * Returns current status after waiting for the pending transmit/receive
 */
uint8_t i2c_wait_busy()
{
	while(i2c_busy())
	{
		#ifdef DEBUG
			PORTB ^= (1 << PB1);
		#endif
	}
	return i2c_status;
}

/**
 * Returns current status after waiting for the currently active transmission
 */
uint8_t i2c_wait_busy_int()
{
	while(i2c_busy_int())
	{
		#ifdef DEBUG
			PORTB ^= (1 << PB1);
		#endif
	}
	return i2c_status;
}

/**
 * Send provided data the next time this slave is addressed with a SLA+W
 *
 * Also enables RX Acknowledge
 */
void i2c_slave_start_tx_data(uint8_t * msg, uint8_t size)
{
	uint8_t tmp;

	if (size > I2C_BUFFER_SIZE)
	{
		size = I2C_BUFFER_SIZE;
	}

	//Wait until whatever was done before is finished
	while(i2c_busy_int())
	{
		#ifdef DEBUG
			PORTB ^= (1 << PB1);
		#endif
	}

	i2c_msg_size = size;

	//TODO: potentially make atomic
	for(tmp = 0; tmp < size && tmp < I2C_BUFFER_SIZE; tmp++)
	{
		i2c_tx_buf[tmp] = msg[tmp];
	}
	//i2c_status_register.all = 0;
	i2c_status_register.tx_data_in_buf = 1;
	i2c_status_register.last_tx_ok = 0;
	i2c_status = I2C_NO_STATE;

	// Reset transmit buffer pointer:
	i2c_tx_buf_ptr = 0;

	// Enable i2c, enable interrupt, clear interrupt flag
	// Respond with ACK, the next time we are called
	TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT) | (1 << TWEA);
}

/**
 * Enables RX Mode for this slave. Also enables TX of whatever is in the buffer
 */
void i2c_slave_start_rx()
{
	while(i2c_busy_int())
	{
		#ifdef DEBUG
			PORTB ^= (1 << PB1);
		#endif
	}

	i2c_status_register.gen_call_rx = 0;
	i2c_status_register.last_rx_ok = 0;
	i2c_status_register.rx_data_in_buf = 0;

	i2c_status = I2C_NO_STATE;

	// Enable i2c, enable interrupt, clear interrupt flag
	// Respond with ACK, the next time we are called
	TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT) | (1 << TWEA);
}

uint8_t i2c_get_data(uint8_t * msg, uint8_t size)
{
	uint8_t i;

	if (size > I2C_BUFFER_SIZE)
	{
		for(i = I2C_BUFFER_SIZE; i < size; ++i)
		{
			msg[i] = 0x00;
		}

		size = I2C_BUFFER_SIZE;
	}

	while(i2c_busy_int())
	{
		#ifdef DEBUG
			PORTB ^= (1 << PB1);
		#endif
	}

	if(i2c_status_register.last_rx_ok)
	{
		for(i = 0; i < size; ++i)
		{
			msg[i] = i2c_rx_buf[i];
		}
		i2c_status_register.rx_data_in_buf = 0;
	}
	return i2c_status_register.last_rx_ok;
}

