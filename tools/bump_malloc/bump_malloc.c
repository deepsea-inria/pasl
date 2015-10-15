#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <locale.h>
#include <dlfcn.h>
#include <pthread.h>

#include "bump_malloc.h"

pthread_key_t thread_tail_pointer;
pthread_key_t thread_cur_pointer;

/* option to use gcc's intrinsics to do thread-safe statistics operations */
#define THREAD_SAFE_GCC_INTRINSICS      0

/* function pointer to the real procedures, loaded using dlsym */
typedef void* (*malloc_type)(size_t);
typedef void  (*free_type)(void*);
typedef void* (*realloc_type)(void*, size_t);

static malloc_type real_malloc = NULL;
static free_type real_free = NULL;
static realloc_type real_realloc = NULL;

/* a sentinel value prefixed to each allocation */
static const size_t sentinel = 0xDEADC0DE;

/* a simple memory heap for allocations prior to dlsym loading */
#define INIT_HEAP_SIZE 1024*1024
static char init_heap[INIT_HEAP_SIZE];
static size_t init_heap_use = 0;

/*****************************************/
/* run-time memory allocation statistics */
/*****************************************/

static bump_malloc_callback_type callback = NULL;
static void* callback_cookie = NULL;

/* user function to supply a memory profile callback */
void bump_malloc_set_callback(bump_malloc_callback_type cb, void* cookie)
{
    callback = cb;
    callback_cookie = cookie;
}

const size_t malloc_szb = 1 << 25;

void allocate_new_block() {
  char* new_head = (char*)real_malloc(malloc_szb);
  if (new_head == NULL) {
    fprintf(stderr,"bump_malloc ### C heap full !!!\n");
    exit(EXIT_FAILURE);
  }
  char* new_tail = new_head + malloc_szb;
  pthread_setspecific(thread_tail_pointer, (void*)new_tail);
  pthread_setspecific(thread_cur_pointer, (void*)new_head);
}

char* bump_by(char* pointer, size_t size) {
  return pointer + size;
}

/****************************************************/
/* exported symbols that overlay the libc functions */
/****************************************************/

/* exported malloc symbol that overrides loading from libc */
extern void* malloc(size_t size)
{
    void* ret;

    if (size == 0) return NULL;

    if (real_malloc)
    {
      if (size >= malloc_szb) {
        fprintf(stderr,"bump_malloc ### shouldn't happen !!!\n");
        exit(EXIT_FAILURE);
        return (*real_malloc)(malloc_szb);
      }

      char* tail = (char*)pthread_getspecific(thread_tail_pointer);
      if (tail == NULL) {
        allocate_new_block();
        tail = (char*)pthread_getspecific(thread_tail_pointer);
      }
      if (tail == NULL) {
        fprintf(stderr,"bump_malloc ### failed to allocate new block !!!\n");
        exit(EXIT_FAILURE);
      }
      char* cur = (char*)pthread_getspecific(thread_cur_pointer);
      if (cur == NULL) {
        fprintf(stderr,"bump_malloc ### bogus value of cur !!!\n");
        exit(EXIT_FAILURE);
      }
      // todo: pad size up to cache line
      char* new_cur = bump_by(cur, size);
      if (new_cur >= tail) {
        allocate_new_block();
        tail = (char*)pthread_getspecific(thread_tail_pointer);
        if (tail == NULL) {
          fprintf(stderr,"bump_malloc ### failed to allocate new block !!!\n");
          exit(EXIT_FAILURE);
        }
        cur = (char*)pthread_getspecific(thread_cur_pointer);
        new_cur = bump_by(cur, size);
      }
      if (new_cur >= tail) {
        fprintf(stderr,"bump_malloc ### failed to allocate new block !!!\n");
        exit(EXIT_FAILURE);
      }      
      pthread_setspecific(thread_cur_pointer, (void*)new_cur);

      return (void*)cur;
      /*      

        ret = (*real_malloc)(2*sizeof(size_t) + size);


        ((size_t*)ret)[0] = size;
        ((size_t*)ret)[1] = sentinel;

        return (char*)ret + 2*sizeof(size_t);
      */
    }
    else
    {
        if (init_heap_use + sizeof(size_t) + size > INIT_HEAP_SIZE) {
            fprintf(stderr,"bump_malloc ### init heap full !!!\n");
            exit(EXIT_FAILURE);
        }

        ret = init_heap + init_heap_use;
        init_heap_use += 2*sizeof(size_t) + size;

        /* prepend allocation size and check sentinel */
        ((size_t*)ret)[0] = size;
        ((size_t*)ret)[1] = sentinel;

        return (char*)ret + 2*sizeof(size_t);
    }
}

/* exported free symbol that overrides loading from libc */
extern void free(void* ptr)
{
  /*
  
    size_t size;

    if (!ptr) return; 

    if ((char*)ptr >= init_heap &&
        (char*)ptr <= init_heap + init_heap_use)
    {
        return;
    }

    if (!real_free) {
        fprintf(stderr,"bump_malloc ### free(%p) outside init heap and without real_free !!!\n", ptr);
        return;
    }

    ptr = (char*)ptr - 2*sizeof(size_t);

    if (((size_t*)ptr)[1] != sentinel) {
        fprintf(stderr,"bump_malloc ### free(%p) has no sentinel !!! memory corruption?\n", ptr);
    }

    size = ((size_t*)ptr)[0];

    (*real_free)(ptr); */
}

/* exported calloc() symbol that overrides loading from libc, implemented using our malloc */
extern void* calloc(size_t nmemb, size_t size)
{
    void* ret;
    size *= nmemb;
    if (!size) return NULL;
    ret = malloc(size);
    memset(ret, 0, size);
    return ret;
}

#if 0
/* exported realloc() symbol that overrides loading from libc */
extern void* realloc(void* ptr, size_t size)
{
    void* newptr;
    size_t oldsize;

    if ((char*)ptr >= (char*)init_heap &&
        (char*)ptr <= (char*)init_heap + init_heap_use)
    {

        ptr = (char*)ptr - 2*sizeof(size_t);

        if (((size_t*)ptr)[1] != sentinel) {
            fprintf(stderr,"bump_malloc ### realloc(%p) has no sentinel !!! memory corruption?\n", ptr);
        }

        oldsize = ((size_t*)ptr)[0];

        if (oldsize >= size) {
            /* keep old area, just reduce the size */
            ((size_t*)ptr)[0] = size;
            return (char*)ptr + 2*sizeof(size_t);
        }
        else {
            /* allocate new area and copy data */
            ptr = (char*)ptr + 2*sizeof(size_t);
            newptr = malloc(size);
            memcpy(newptr, ptr, oldsize);
            free(ptr);
            return newptr;
        }
    }

    if (size == 0) { /* special case size == 0 -> free() */
        free(ptr);
        return NULL;
    }

    if (ptr == NULL) { /* special case ptr == 0 -> malloc() */
        return malloc(size);
    }

    ptr = (char*)ptr - 2*sizeof(size_t);

    if (((size_t*)ptr)[1] != sentinel) {
        fprintf(stderr,"bump_malloc ### free(%p) has no sentinel !!! memory corruption?\n", ptr);
    }

    oldsize = ((size_t*)ptr)[0];

    newptr = (*real_realloc)(ptr, 2*sizeof(size_t) + size);

    ((size_t*)newptr)[0] = size;

    return (char*)newptr + 2*sizeof(size_t);
}
#endif

static __attribute__((constructor)) void init(void)
{
    char *error;

    pthread_key_create(&thread_tail_pointer, NULL);
    pthread_key_create(&thread_cur_pointer, NULL);
    
    setlocale(LC_NUMERIC, ""); /* for better readable numbers */

    dlerror();

    real_malloc = (malloc_type)dlsym(RTLD_NEXT, "malloc");
    if ((error = dlerror()) != NULL) {
        fprintf(stderr, "bump_malloc ### error %s\n", error);
        exit(EXIT_FAILURE);
    }

    /*    real_realloc = (realloc_type)dlsym(RTLD_NEXT, "realloc");
    if ((error = dlerror()) != NULL) {
        fprintf(stderr, "bump_malloc ### error %s\n", error);
        exit(EXIT_FAILURE);
        } */

    real_free = (free_type)dlsym(RTLD_NEXT, "free");
    if ((error = dlerror()) != NULL) {
        fprintf(stderr, "bump_malloc ### error %s\n", error);
        exit(EXIT_FAILURE);
    }
}

static __attribute__((destructor)) void finish(void) {

}
