#ifndef _CRC_H
#define _CRC_H
#include <stdint.h>
unsigned long crc32(const unsigned char *s, unsigned int len);
uint32_t uint32_hash(const unsigned char *s, unsigned int len);
uint32_t long_hash(unsigned long x);
#endif
