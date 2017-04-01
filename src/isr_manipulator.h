/*
 * isr_manipulator.h
 *
 *  Created on: 26.02.2017
 *      Author: martin
 */

#ifndef SRC_ISR_MANIPULATOR_H_
#define SRC_ISR_MANIPULATOR_H_

#ifdef __AVR_ATmega8__
	#define ISR_REGISTER GICR
#else
	#ifdef __AVR_ATmega88__
		#define ISR_REGISTER MCUCR
	#else
		#error Incompatible AVR MCU. Please configure correct ISR_REGISTER
	#endif
#endif

#include <avr/io.h>

inline void isr_fbl()
{
	// Adjust ISR vector (default is app ISR vector)
	uint8_t tmp;
	tmp = ISR_REGISTER;

	ISR_REGISTER = tmp | (1 << IVCE);
	ISR_REGISTER = tmp | (1 << IVSEL);
}

inline void isr_app()
{
	uint8_t tmp;
	// Return the ISR vector back to normal
	tmp = ISR_REGISTER;
	ISR_REGISTER = tmp | (1 << IVCE);
	ISR_REGISTER = tmp & ~(1 << IVSEL);
}

#endif /* SRC_ISR_MANIPULATOR_H_ */
