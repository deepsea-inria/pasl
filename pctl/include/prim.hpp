/* COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file prim.hpp
 * \brief Basic allocation and memory transfer operations
 *
 */

#include <limits.h>
#include <memory>
#include <utility>
#ifndef TARGET_MAC_OS
#include <malloc.h>
#endif
#include <algorithm>

#include "granularity.hpp"

#ifndef _PARRAY_PCTL_PRIM_H_
#define _PARRAY_PCTL_PRIM_H_

/***********************************************************************/

namespace pasl {
namespace pctl {
  
/*---------------------------------------------------------------------*/

namespace par = sched::granularity;
using controller_type = par::control_by_prediction;
//using controller_type = par::control_by_force_parallel;
using loop_controller_type = par::loop_by_eager_binary_splitting<controller_type>;

template <class T>
std::string string_of_template_arg() {
  return std::string(typeid(T).name());
}

template <class T>
std::string sota() {
  return string_of_template_arg<T>();
}

} // end namespace
} // end namespace

namespace pasl {
namespace pctl {
namespace prim {
  
/*---------------------------------------------------------------------*/
/* Primitive memory operations */

template <class T>
T* alloc_array(long n) {
  return (T*)malloc(n*sizeof(T));
}
  
template <class Forward_iterator, class T>
class fill_controller_type {
public:
  static controller_type contr;
};

template <class Forward_iterator, class T>
controller_type fill_controller_type<Forward_iterator,T>::contr(
  "fill"+sota<Forward_iterator>()+sota<T>());
  
template <class Forward_iterator, class T>
void fill(Forward_iterator lo, Forward_iterator hi, const T& val) {
  using controller_type = fill_controller_type<Forward_iterator, T>;
  long nb = hi - lo;
  auto seq = [&] {
    std::fill(lo, hi, val);
  };
  par::cstmt(controller_type::contr, [&] { return nb; }, [&] {
    if (nb <= 1) {
      seq();
    } else {
      long mid = nb / 2;
      par::fork2([&] {
        fill(lo, lo + mid, val);
      }, [&] {
        fill(lo + mid, hi, val);
      });
    }
  }, seq);
}

template <class Input_iterator, class Output_iterator>
class copy_controller_type {
public:
static controller_type contr;
};

template <class Input_iterator, class Output_iterator>
controller_type copy_controller_type<Input_iterator,Output_iterator>::contr(
  "copy"+sota<Input_iterator>()+sota<Output_iterator>());

template <class Input_iterator, class Output_iterator>
void copy(Input_iterator lo, Input_iterator hi, Output_iterator dst) {
  using controller_type = copy_controller_type<Input_iterator,Output_iterator>;
  long nb = hi - lo;
  auto seq = [&] {
    std::copy(lo, hi, dst);
  };
  par::cstmt(controller_type::contr, [&] { return nb; }, [&] {
    if (nb <= 1) {
      seq();
    } else {
      long mid = nb / 2;
      par::fork2([&] {
        copy(lo, lo + mid, dst);
      }, [&] {
        copy(lo + mid, hi, dst + mid);
      });
    }
  }, seq);
}
  
template <class T>
class pdelete_controller_type {
public:
  static controller_type contr;
};

template <class T>
controller_type pdelete_controller_type<T>::contr(
  "pdelete"+sota<T>());

template <class T, class Alloc>
void pdelete(T* lo, T* hi) {
  using controller_type = pdelete_controller_type<T>;
  long nb = hi - lo;
  Alloc alloc;
  auto seq = [&] {
    for (T* i = lo; i < hi; i++) {
      alloc.destroy(i);
    }
  };
  par::cstmt(controller_type::contr, [&] { return nb; }, [&] {
    if (nb <= 1) {
      seq();
    } else {
      long mid = nb / 2;
      par::fork2([&] {
        pdelete<T, Alloc>(lo, lo + mid);
      }, [&] {
        pdelete<T, Alloc>(lo + mid, hi);
      });
    }
  }, seq);
}
  
/***********************************************************************/

} // end namespace
} // end namespace
} // end namespace


#endif /*! _PARRAY_PCTL_PRIM_H_ */