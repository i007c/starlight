
#ifndef __STARLIGHT_COMMON_H__
#define __STARLIGHT_COMMON_H__

#include "starlight.h"

uint16_t u16_be(Starlight *starlight);
uint32_t u32_be(Starlight *starlight);


uint32_t calc_crc(uint8_t *buffer, uint64_t length);

#endif // __STARLIGHT_COMMON_H__
