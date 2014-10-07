/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file control.hpp
 * \brief Control operators
 *
 */

#include <signal.h>
#include <cstdlib>
#include <assert.h>

namespace pasl {
namespace util {
namespace control {
  
/***********************************************************************/

#ifndef _PASL_CONTROL_H_
#define _PASL_CONTROL_H_

static constexpr int thread_stack_szb = 1<<20;
  
#if defined(TARGET_MAC_OS) || defined(USE_UCONTEXT)
 
  // on MAC OS need to define _XOPEN_SOURCE to access setcontext
#include <ucontext.h>
  
class context {
private:
  
  struct context_struct {
    ucontext_t ucxt;
    void* val;
  };
  
public:
  
  using context_type = struct context_struct;
  using context_pointer = context_type*;
  
  static inline context_pointer addr(context_type& r) {
    return &r;
  }
  
  template <class Value>
  static Value capture(context_pointer cxt) {
    cxt->val = nullptr;
    getcontext(&(cxt->ucxt));
    return (Value)(cxt->val);
  }
  
  template <class Value>
  static void throw_to(context_pointer cxt, Value val) {
    cxt->val = (Value)val;
    setcontext(&(cxt->ucxt));
  }
  
  template <class Value>
  static void swap(context_pointer cxt1, context_pointer cxt2, Value val2) {
    cxt2->val = val2;
    swapcontext(&(cxt1->ucxt), &(cxt2->ucxt));
  }
  
  template <class Value>
  static char* spawn(context_pointer cxt, Value val) {
    char* stack = (char*)malloc(thread_stack_szb);
    Value val2 = capture<Value>(cxt);
    cxt->ucxt.uc_link = nullptr;
    cxt->ucxt.uc_stack.ss_sp = stack;
    cxt->ucxt.uc_stack.ss_size = thread_stack_szb;
    auto enter_func = (void (*)(void)) val->enter;
    makecontext(&(cxt->ucxt), enter_func, 1, val);
    return stack;
  }
  
};
  
#else

using _context_pointer = char*;

extern "C" void* _pasl_cxt_save(_context_pointer cxt);
extern "C" void _pasl_cxt_restore(_context_pointer cxt, void* t);
  
class context {  
public:
  
  typedef char context_type[8*8];
  using context_pointer = _context_pointer;
  
  template <class X>
  static context_pointer addr(X r) {
    return r;
  }
  
  template <class Value>
  static void throw_to(context_pointer cxt, Value val) {
    _pasl_cxt_restore(cxt, (void*)val);
  }
  
  template <class Value>
  static void swap(context_pointer cxt1, context_pointer cxt2, Value val2) {
    if (_pasl_cxt_save(cxt1))
      return;
    _pasl_cxt_restore(cxt2, val2);
  }
  
  // register number 6
#define _X86_64_SP_OFFSET   6
  
  template <class Value>
  static Value capture(context_pointer cxt) {
    void* r = _pasl_cxt_save(cxt);
    return (Value)r;
  }
  
  template <class Value>
  static char* spawn(context_pointer cxt, Value val) {
    Value target;
    if (target = (Value)_pasl_cxt_save(cxt)) {
      target->enter(target);
      assert(false);
    }
    char* stack = (char*)malloc(thread_stack_szb);
    void** _cxt = (void**)cxt;
    _cxt[_X86_64_SP_OFFSET] = &stack[thread_stack_szb];
    return stack;
  }
  
};

  
#endif
  
context::context_pointer my_cxt();

#endif /*! _PASL_CONTROL_H_ */
  
/***********************************************************************/
  
} // end namespace
} // end namespace
} // end namespace
