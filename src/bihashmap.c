/*
 * This hashmap is based on the hashmap in hashmap.c
 * It differs in several key aspects:
 *    * Keys are bipartite and consist of a void* (any_t) and an integer
 *    * The value is always a uintptr_t
 *    * The caller must provide a uint32_t hash key
 *    * The caller must provide a function pointer int compare(any_t, int, uintptr_t)
 */
#include "bihashmap.h"
#include "crc.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define INITIAL_SIZE (256)
#define MAX_CHAIN_LENGTH (8)

/* We need to keep keys and values */
typedef struct _hashmap_element
{
   any_t key;
   int in_use;
   uint32_t hashcode;
   uintptr_t data;
} hashmap_element;

/* A hashmap has some maximum size and current size,
 * as well as the data to hold. */
typedef struct _hashmap_map
{
   int table_size;
   int size;
   compare1_t compare1;
   compare2_t compare2;
   hashmap_element *data;
} hashmap_map;

/*
 * Return an empty hashmap, or NULL on failure.
 */
bimap_t bihashmap_new(compare1_t compare1, compare2_t compare2)
{
   hashmap_map* m = (hashmap_map*) malloc(sizeof(hashmap_map));
   if(!m) goto err;
   m->data = (hashmap_element*) calloc(INITIAL_SIZE, sizeof(hashmap_element));
   if(!m->data) goto err;

   m->table_size = INITIAL_SIZE;
   m->size = 0;
   m->compare1 = compare1;
   m->compare2 = compare2;

   return m;
  err:
   if (m)
      bihashmap_free(m);
   return NULL;
}


/*
 * Return the integer of the location in data
 * to store the point to the item, or MAP_FULL.
 */
int bihashmap_hash2(bimap_t in, uint32_t hashcode, void* key1, int key2){
	int curr;
	int i;

	/* Cast the hashmap */
	hashmap_map* m = (hashmap_map *) in;

	/* If full, return immediately */
        if(m->size >= (m->table_size/2))
           return MAP_FULL;

	/* Find the best index */
        curr = hashcode % m->table_size;

	/* Linear probing */
        for(i = 0; i< MAX_CHAIN_LENGTH; i++)
        {
           if(m->data[curr].in_use == 0)
              return curr;
           if(m->data[curr].in_use == 1 && m->compare2(key1, key2, m->data[curr].data) == 1)
              return curr;
           curr = (curr + 1) % m->table_size;
        }
	return MAP_FULL;
}

int bihashmap_hash1(bimap_t in, uint32_t hashcode, any_t key){
	int curr;
	int i;

	/* Cast the hashmap */
	hashmap_map* m = (hashmap_map *) in;

	/* If full, return immediately */
        if(m->size >= (m->table_size/2))
           return MAP_FULL;

	/* Find the best index */
        curr = hashcode % m->table_size;

	/* Linear probing */
        for(i = 0; i< MAX_CHAIN_LENGTH; i++)
        {
           if(m->data[curr].in_use == 0)
              return curr;
           if(m->data[curr].in_use == 1 && m->compare1(key, m->data[curr].data) == 1)
              return curr;
           curr = (curr + 1) % m->table_size;
        }
	return MAP_FULL;
}

/*
 * Doubles the size of the hashmap, and rehashes all the elements
 */
int bihashmap_rehash(bimap_t in)
{
   int i;
   int old_size;
   hashmap_element* curr;

   /* Setup the new elements */
   hashmap_map *m = (hashmap_map *) in;
   hashmap_element* temp = (hashmap_element *)calloc(2 * m->table_size, sizeof(hashmap_element));
   if(!temp) return MAP_OMEM;

   /* Update the array */
   curr = m->data;
   m->data = temp;

   /* Update the size */
   old_size = m->table_size;
   m->table_size = 2 * m->table_size;
   m->size = 0;

   /* Rehash the elements */
   for(i = 0; i < old_size; i++){
      int status;

      if (curr[i].in_use == 0)
         continue;
      status = bihashmap_put(m, curr[i].hashcode, curr[i].key, curr[i].data);
      if (status != MAP_OK)
         return status;
   }

   free(curr);

   return MAP_OK;
}

/*
 * Add a pointer to the hashmap with some key
 */
int bihashmap_put(bimap_t in, uint32_t hashcode, any_t key, uintptr_t value)
{
   int index;
   hashmap_map* m;

   /* Cast the hashmap */
   m = (hashmap_map *) in;

   /* Find a place to put our value */
   index = bihashmap_hash1(in, hashcode, key);
   while(index == MAP_FULL)
   {
      if (bihashmap_rehash(in) == MAP_OMEM)
         return MAP_OMEM;
      index = bihashmap_hash1(in, hashcode, key);
   }
   /* Set the data */
   m->data[index].data = value;
   m->data[index].key = key;
   m->data[index].in_use = 1;
   m->data[index].hashcode = hashcode;
   m->size++;
   return MAP_OK;
}

void bihashmap_check(bimap_t in)
{
   hashmap_map* m;
   m = (hashmap_map *) in;
   printf("XXX %p (%p)\n", m->data, &m->data);
   assert(m->data > (void*)0x1000);
}

/*
 * Get your pointer out of the hashmap with a key
 */
int bihashmap_get(bimap_t in, uint32_t hashcode, void* key1, int key2, uintptr_t *arg)
{
   int curr;
   int i;
   hashmap_map* m;

   /* Cast the hashmap */
   m = (hashmap_map *) in;

   /* Find data location */
   curr = hashcode % m->table_size;
   /* Linear probing, if necessary */
   for(i = 0; i<MAX_CHAIN_LENGTH; i++)
   {
        int in_use = m->data[curr].in_use;
        if (in_use == 1)
        {
           if (m->compare2(key1, key2, m->data[curr].data))
           {
              *arg = (m->data[curr].data);
              return MAP_OK;
            }
        }
        curr = (curr + 1) % m->table_size;
   }

   *arg = 0;

   /* Not found */
   return MAP_MISSING;
}

/*
 * Remove an element with that key from the map
 */
int bihashmap_remove(bimap_t in, uint32_t hashcode, void* key1, int key2){
   int i;
   int curr;
   hashmap_map* m;

   /* Cast the hashmap */
   m = (hashmap_map *) in;

   /* Find key */
   curr = hashcode % m->table_size;

   /* Linear probing, if necessary */
   for(i = 0; i<MAX_CHAIN_LENGTH; i++)
   {
      int in_use = m->data[curr].in_use;
      if (in_use == 1)
      {
         if (m->compare2(key1, key2, m->data[curr].data))
         {
            /* Blank out the fields */
            m->data[curr].in_use = 0;
            m->data[curr].data = 0;
            m->data[curr].key = NULL;
            /* Reduce the size */
            m->size--;
            return MAP_OK;
         }
      }
      curr = (curr + 1) % m->table_size;
   }

   /* Data not found */
   return MAP_MISSING;
}

/* Deallocate the hashmap */
void bihashmap_free(bimap_t in)
{
   hashmap_map* m = (hashmap_map*) in;
   free(m->data);
   free(m);
}

/* Return the length of the hashmap */
int bihashmap_length(bimap_t in)
{
   hashmap_map* m = (hashmap_map *) in;
   if(m != NULL) return m->size;
   else return 0;
}
