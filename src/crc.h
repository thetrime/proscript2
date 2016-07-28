#ifndef _CRC_H
#define _CRC_H
#include <stdint.h>
unsigned long crc32(const unsigned char *s, unsigned int len);
uint32_t hash(const unsigned char *s, unsigned int len);
#endif
