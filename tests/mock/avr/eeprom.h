/* Host mock of <avr/eeprom.h>: EEPROM is the array mock_eeprom[]. */
#ifndef MOCK_AVR_EEPROM_H
#define MOCK_AVR_EEPROM_H
#include <stdint.h>
#define MOCK_EEPROM_SIZE 512
extern uint8_t mock_eeprom[MOCK_EEPROM_SIZE];
uint8_t eeprom_read_byte(const uint8_t *addr);
#define eeprom_busy_wait() ((void) 0)
#endif
