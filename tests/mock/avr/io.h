/*
 * Host mock of <avr/io.h> for the PC test suite (atmega8 register subset).
 * Registers are plain variables defined in tests/mock/fbl_mock.c.
 */
#ifndef MOCK_AVR_IO_H
#define MOCK_AVR_IO_H

#include <stdint.h>

extern volatile uint8_t PORTB, DDRB, GICR, SREG;

#define PB0 0
#define PB1 1

/* GICR bits (atmega8) */
#define IVCE  0
#define IVSEL 1

#endif
