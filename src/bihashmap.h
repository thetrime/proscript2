/*
 * Generic hashmap manipulation functions
 *
 * Originally by Elliot C Back - http://elliottback.com/wp/hashmap-implementation-in-c/
 *
 * Modified by Pete Warden to fix a serious performance problem, support strings as keys
 * and removed thread synchronization - http://petewarden.typepad.com
 */
#ifndef __BIHASHMAP_H__
#define __BIHASHMAP_H__

#define MAP_MISSING -3  /* No such element */
#define MAP_FULL -2 	/* Hashmap is full */
#define MAP_OMEM -1 	/* Out of Memory */
#define MAP_OK 0 	/* OK */

#include <stdint.h>

/*
 * any_t is a pointer.  This allows the first key to be arbitrary data
 */
typedef void *any_t;

/*
 * PBFany is a pointer to a function that can take a bipartite key
 * and return an integer. Returns status code..
 */
typedef int (*PBFany)(any_t, any_t, int, uintptr_t);

/*
 * map_t is a pointer to an internally maintained data structure.
 * Clients of this package do not need to know how hashmaps are
 * represented.  They see and manipulate only bimap_t's.
 */
typedef any_t bimap_t;


/*
 * compare_t is a function pointer that can be used to compare a key to check
 * for hash conflicts
 */
typedef int(*compare1_t)(any_t, uintptr_t);
typedef int(*compare2_t)(any_t, int, uintptr_t);

/*
 * Return an empty hashmap. Returns NULL if empty.
*/
extern bimap_t bihashmap_new(compare1_t, compare2_t);

extern void bihashmap_check(bimap_t);

/*
 * Iteratively call f with argument (item, key1, key2, data) for
 * each element data in the hashmap. The function must
 * return a map status code. If it returns anything other
 * than MAP_OK the traversal is terminated. f must
 * not reenter any hashmap functions, or deadlock may arise.
 */
extern int bihashmap_iterate(bimap_t in, PBFany f, any_t item);

/*
 * Add an element to the hashmap. Return MAP_OK or MAP_OMEM.
 */
extern int bihashmap_put(bimap_t in, uint32_t hashcode, any_t key, uintptr_t value);

/*
 * Get an element from the hashmap. Return MAP_OK or MAP_MISSING.
 */
extern int bihashmap_get(bimap_t in, uint32_t hashcode, any_t key1, int key2, uintptr_t *arg);

/*
 * Remove an element from the hashmap. Return MAP_OK or MAP_MISSING.
 */
extern int bihashmap_remove(bimap_t in, uint32_t hashcode, any_t key1, int key2);

/*
 * Free the hashmap
 */
extern void bihashmap_free(bimap_t in);

/*
 * Get the current size of a hashmap
 */
extern int bihashmap_length(bimap_t in);

#endif // __HASHMAP_H__
