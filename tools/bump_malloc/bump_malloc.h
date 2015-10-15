#ifndef _BUMP_MALLOC_H_
#define _BUMP_MALLOC_H_

#include <stdlib.h>

#ifdef __cplusplus
extern "C" { /* for inclusion from C++ */
#endif

/* typedef of callback function */
typedef void (*bump_malloc_callback_type)(void* cookie, size_t current);

/* supply bump_malloc with a callback function that is invoked on each change
 * of the current allocation. The callback function must not use
 * malloc()/realloc()/free() or it will go into an endless recursive loop! */
extern void bump_malloc_set_callback(bump_malloc_callback_type cb, void* cookie);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _BUMP_MALLOC_H_ */
