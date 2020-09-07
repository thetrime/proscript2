#include "hash.h"

uint32_t hash64(uint64_t key)
{
   key = (~key) + (key << 18);
   key = key ^ (key >> 31);
   key = key * 21;
   key = key ^ (key >> 11);
   key = key + (key << 6);
   key = key ^ (key >> 22);
   return (uint32_t) key;
}

uint32_t hash32(uint32_t key)
{
   key = ((key >> 16) ^ key) * 0x45d9f3b;
   key = ((key >> 16) ^ key) * 0x45d9f3b;
   key = (key >> 16) ^ key;
   return key;
}

#ifdef mpz_limbs_read
uint32_t hashmpz(mpz_t key)
{
   mp_limb_t hash = 0;
   const mp_limb_t * limbs =  mpz_limbs_read(key);
   for (int i = 0; i < mpz_size(key); i++)
      hash ^= limbs[i];
   if (sizeof(mp_limb_t) == 8)
      return hash64(hash);
   else if (sizeof(mp_limb_t) == 4)
      return hash32(hash);
   else
      assert(0 && "Cannot deal with mp_limb size");
}
#else
uint32_t hashmpz(mpz_t key)
{
   // Oh well, we tried
   char* str = mpz_get_str(NULL, 10, key);
   uint32_t result = uint32_hash((unsigned char*)str, strlen(str));
   free(str);
   return result;
}
#endif

uint32_t hashmpq(mpq_t key)
{
   uint32_t h = 0;
   mpz_t n;
   mpz_init(n);
   mpq_get_num(n, key);
   h = hashmpz(n);
   mpz_clear(n);
   mpz_init(n);
   mpq_get_den(n, key);
   h ^= hashmpz(n);
   mpz_clear(n);
   return h;
}
