#ifndef _HASH_H
#define _HASH_H

#include <gmp.h>
#include <stdlib.h>
#include <assert.h>
#include "crc.h"

uint32_t hash64(uint64_t key);
uint32_t hash32(uint32_t key);
uint32_t hashmpz(mpz_t key);
uint32_t hashmpq(mpq_t key);

#endif
