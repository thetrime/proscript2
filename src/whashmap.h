/*
 * Generic hashmap manipulation functions
 *
 * Originally by Elliot C Back - http://elliottback.com/wp/hashmap-implementation-in-c/
 *
 * Modified by Pete Warden to fix a serious performance problem, support strings as keys
 * and removed thread synchronization - http://petewarden.typepad.com
 */
#ifndef __WHASHMAP_H__
#define __WHASHMAP_H__

#include <stdint.h>
typedef uintptr_t word;

#define MAP_MISSING -3  /* No such element */
#define MAP_FULL -2 	/* Hashmap is full */
#define MAP_OMEM -1 	/* Out of Memory */
#define MAP_OK 0 	/* OK */

/*
 * any_t is a pointer.  This allows you to put arbitrary structures in
 * the hashmap.
 */
typedef void *any_t;

/*
 * PWFany is a pointer to a function that can take two any_t arguments
 * and return an integer. Returns status code..
 */
typedef int (*PWFany)(any_t, word, any_t);

/*
 * wmap_t is a pointer to an internally maintained data structure.
 * Clients of this package do not need to know how hashmaps are
 * represented.  They see and manipulate only wmap_t's.
 */
typedef any_t wmap_t;

/*
 * Return an empty hashmap. Returns NULL if empty.
*/
extern wmap_t whashmap_new();

/*
 * Iteratively call f with argument (item, data) for
 * each element data in the hashmap. The function must
 * return a map status code. If it returns anything other
 * than MAP_OK the traversal is terminated. f must
 * not reenter any hashmap functions, or deadlock may arise.
 */
extern int whashmap_iterate(wmap_t in, PWFany f, any_t item);

/*
 * Add an element to the hashmap. Return MAP_OK or MAP_OMEM.
 */
extern int whashmap_put(wmap_t in, word key, any_t value);

/*
 * Get an element from the hashmap. Return MAP_OK or MAP_MISSING.
 */
extern int whashmap_get(wmap_t in, word key, any_t *arg);

/*
 * Remove an element from the hashmap. Return MAP_OK or MAP_MISSING.
 */
extern int whashmap_remove(wmap_t in, word key);

/*
 * Get any element. Return MAP_OK or MAP_MISSING.
 * remove - should the element be removed from the hashmap
 */
extern int whashmap_get_one(wmap_t in, any_t *arg, int remove);

/*
 * Free the hashmap
 */
extern void whashmap_free(wmap_t in);

/*
 * Get the current size of a hashmap
 */
extern int whashmap_length(wmap_t in);

#endif // __WHASHMAP_H__
