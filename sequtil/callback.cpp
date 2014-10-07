/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file callback.hpp
 * \brief Interface for registering initialization/destruction actions
 * with the PASL runtime
 *
 */

#include <assert.h>

#include "atomic.hpp"
#include "callback.hpp"

/***********************************************************************/

namespace pasl {
namespace util {
namespace callback {
  
/*!\class myset
 * \brief A data structure to represent a set of a given type.
 *
 * \remark The maximum size of the set is determined by the template
 * parameter max_sz.
 *
 */
template <class Elt, int max_sz>
class myset {
private:
  int i;
  Elt inits[max_sz];
  
public:
  int size() {
    return i;
  }
  
  void push(Elt init) {
    if (i >= max_sz)
      atomic::die("need to increase max_sz\n");
    inits[i] = init;
    i++;
  }
  
  Elt peek(size_t i) {
    assert(i < size());
    return inits[i];
  }
  
  Elt pop() {
    i--;
    Elt x = inits[i];
    inits[i] = Elt();
    return x;
  }
  
};
  
myset<client_p, 2048> callbacks;
  
void init() {
  for (int i = 0; i < callbacks.size(); i++) {
    client_p callback = callbacks.peek(i);
    callback->init();
  }
}


void output() {
  for (int i = 0; i < callbacks.size(); i++) {
    client_p callback = callbacks.peek(i);
    callback->output();
  }
}

 void destroy() {
  while (callbacks.size() > 0) {
    client_p callback = callbacks.pop();
    callback->destroy();
  }
}

void register_client(client_p c) {
  callbacks.push(c);
}

} // end namespace
} // end namespace
} // end namespace

/***********************************************************************/

