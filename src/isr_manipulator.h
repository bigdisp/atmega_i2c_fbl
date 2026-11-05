/*
 * isr_manipulator.h
 *
 *  Created on: 26.02.2017
 *      Author: martin
 */

#ifndef SRC_ISR_MANIPULATOR_H_
#define SRC_ISR_MANIPULATOR_H_

#if defined(__AVR_ATmega8__)
	#define ISR_REGISTER GICR
#elif defined(__AVR_ATmega88__) || defined(__AVR_ATmega88A__) || \
	defined(__AVR_ATmega88P__) || defined(__AVR_ATmega88PA__)
	#define ISR_REGISTER MCUCR
#else
	#error Incompatible AVR MCU. Please configure correct ISR_REGISTER
#endif

#include <avr/io.h>

static inline void isr_fbl()
{
	// Adjust ISR vector (default is app ISR vector)
	uint8_t tmp;
	tmp = ISR_REGISTER;

	ISR_REGISTER = tmp | (1 << IVCE);
	ISR_REGISTER = tmp | (1 << IVSEL);
}

static inline void isr_app()
{
	uint8_t tmp;
	// Return the ISR vector back to normal
	tmp = ISR_REGISTER;
	ISR_REGISTER = tmp | (1 << IVCE);
	ISR_REGISTER = tmp & ~(1 << IVSEL);
}

#endif /* SRC_ISR_MANIPULATOR_H_ */
