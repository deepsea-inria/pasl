/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file callback.hpp
 * \brief Interface for registering initialization/destruction actions
 * that are to be called by the PASL runtime.
 *
 */

#ifndef _PASL_UTIL_CALLBACK_H_
#define _PASL_UTIL_CALLBACK_H_

/***********************************************************************/

namespace pasl {
namespace util {
namespace callback {

class client {
public:

  virtual void init() = 0;
  
  virtual void destroy() = 0;
  
  virtual void output() = 0;
  
};
  
typedef client* client_p;
  
void init();
void output();
void destroy();
  
void register_client(client_p c);
  
} // end namespace
} // end namespace
} // end namespace

/***********************************************************************/

#endif /*! _PASL_UTIL_CALLBACK_H_ */