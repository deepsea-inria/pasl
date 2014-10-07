/*!
 * \author Umut A. Acar
 * \author Arthur Chargueraud
 * \author Mike Rainey
 * \date 2013-2018
 * \copyright 2014 Umut A. Acar, Arthur Chargueraud, Mike Rainey
 *
 * \brief Fixed capacity vectors
 * \file fixedcapacity.hpp
 *
 */

#include "fixedcapacitybase.hpp"

#ifndef _PASL_DATA_FIXEDCAPACITY_H_
#define _PASL_DATA_FIXEDCAPACITY_H_

namespace pasl {
namespace data {
namespace fixedcapacity {

/***********************************************************************/
  
/*---------------------------------------------------------------------*/
/* Heap-allocated fixed-capacity buffers */

namespace heap_allocated {

  template <class Item, int Capacity, class Alloc = std::allocator<Item>>
  using ringbuffer_ptr = base::ringbuffer_ptr<base::heap_allocator<Item, Capacity+1>>;

  template <class Item, int Capacity, class Alloc = std::allocator<Item>>
  using ringbuffer_ptrx = base::ringbuffer_ptrx<base::heap_allocator<Item, Capacity+1>>;

  template <class Item, int Capacity, class Alloc = std::allocator<Item>>
  using ringbuffer_idx = base::ringbuffer_idx<base::heap_allocator<Item, Capacity>>;

  template <class Item, int Capacity, class Alloc = std::allocator<Item>>
  using stack = base::stack<base::heap_allocator<Item, Capacity>>;

}

/*---------------------------------------------------------------------*/
/* Inline-allocated fixed-capacity arrays */
  
namespace inline_allocated {
  
  template <class Item, int Capacity, class Alloc = std::allocator<Item>>
  using ringbuffer_ptr = base::ringbuffer_ptr<base::inline_allocator<Item, Capacity+1>>;

  template <class Item, int Capacity, class Alloc = std::allocator<Item>>
  using ringbuffer_idx = base::ringbuffer_idx<base::inline_allocator<Item, Capacity>>;

  template <class Item, int Capacity, class Alloc = std::allocator<Item>>
  using stack = base::stack<base::inline_allocator<Item, Capacity>>;
  
}

/***********************************************************************/

} // end namespace
} // end namespace
} // end namespace

#endif /*! _PASL_DATA_FIXEDCAPACITY_H_ */
