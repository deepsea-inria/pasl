/* COPYRIGHT (c) 2011 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * Thread-local storage, using either GCC-specific extension or
 * using p-thread specific functions.
 *
 * Several macros to define a portable interface to thread-local
 * storage. One caveat is that these macros can handle only types that
 * can fit into a void*.
 *
 * Let "ty" be a C type and "name" a C variable identifier.
 *
 * extern_declare(ty, name)
 *
 *   Generates an extern declaration for name. (goes in the header file)
 *
 * global_declare(ty, name)
 *
 *   Generates a global declaration for name. (goes in *one* CPP file)
 *
 * alloc(ty, name)
 *
 *    Generates a C statement which allocates the thread-local storage.
 *
 * dealloc(ty, name)
 *
 *    Generates a C statement which deallocates the thread-local storage.
 *
 * getter(ty, name)
 *
 *    Generates a C expression which returns the value dereferenced
 *    from the thread-local storage.
 *
 * setter(ty, name, x)
 *
 *    Generates a C statement which writes the value x to the
 *    thread-local storage.
 *
 */

/*---------------------------------------------------------------------*/
/* GCC-specific thread-local storage */

#if defined(HAVE_GCC_TLS)

#define __tls_varid(__thename)\
  __ ##  __thename ## _tls

#define tls_extern_declare(__thety, __thename)\
  extern __thread __thety __tls_varid(__thename);

#define tls_global_declare(__thety, __thename)\
  __thread __thety __tls_varid(__thename);

#define tls_alloc(__thety, __thename)\
  ; // does nothing

#define tls_dealloc(__thety, __thename)\
  ; // does nothing

#define tls_setter(__thety, __thename, x)\
  __tls_varid(__thename) = x

#define tls_getter(__thety, __thename)\
  __tls_varid(__thename)

/*---------------------------------------------------------------------*/
/* Pthread-specific thread-local storage */

#else

#include <pthread.h>

#define __tls_key(__thename)\
  __ ##  __thename ## _key

#define tls_extern_declare(__thety, __thename)\
  extern pthread_key_t __tls_key(__thename);

#define tls_global_declare(__thety, __thename)\
  pthread_key_t  __tls_key(__thename); 

#define tls_alloc(__thety, __thename)\
  (void) pthread_key_create (&(__tls_key(__thename)), NULL);

#define tls_dealloc(__thety, __thename)\
  pthread_key_delete (__tls_key(__thename));

#define tls_setter(__thety, __thename, x)\
  pthread_setspecific (__tls_key(__thename), (void*)x);

#define tls_getter(__thety, __thename)\
  (__thety)(size_t)pthread_getspecific (__tls_key(__thename));

#endif

/***********************************************************************/
