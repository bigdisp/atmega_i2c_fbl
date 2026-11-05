/*
 * flash.h
 *
 *  Created on: 26.02.2017
 *      Author: martin
 */

#ifndef SRC_FLASH_H_
#define SRC_FLASH_H_

#include <stdint.h>

/**
 * Erase and write one flash page.
 * @param addr byte address of the page (page aligned)
 * @param content SPM_PAGESIZE bytes
 */
void flash_write_page(uint16_t addr, const uint8_t *content);

#endif /* SRC_FLASH_H_ */
