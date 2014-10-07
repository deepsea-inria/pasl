/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file cldeque.hpp
 *
 */

#include "atomic.hpp"

#ifndef _PASL_DATA_cldeque_H_
#define _PASL_DATA_cldeque_H_

namespace pasl {
namespace data {

/***********************************************************************/
  
/*! \class cldeque
 *  \brief Chase-lev concurrent work-stealing deque
 *  \tparam Word_value type of items to be stored in the container; values
 * of this type must be represented by the same number of bits as is in a
 * machine word
 *  \ingroup data
 *  \ingroup workstealing
 *
 */
template <class Item>
class cldeque {
public:
  
  using value_type = Item*;
  
  using pop_result_type = enum {
    Pop_succeeded,
    Pop_failed_with_empty_deque,
    Pop_failed_with_cas_abort,
    Pop_bogus
  };
  
protected:
  
  using buffer_type = std::atomic<value_type>*;
  
  volatile buffer_type  buf;         // deque contents
  std::atomic<int64_t> capacity;  // maximum number of elements
  std::atomic<int64_t> bottom;    // index of the first unused cell
  std::atomic<int64_t> top;       // index of the last used cell
  
  static value_type cb_get (buffer_type buf, int64_t capacity, int64_t i) {
    return buf[i % capacity].load();
  }
  
  static void cb_put (buffer_type buf, int64_t capacity, int64_t i, value_type x) {
    buf[i % capacity].store(x);
  }
  
  buffer_type new_buffer(size_t capacity) {
    return new std::atomic<value_type>[capacity];
  }
  
  void delete_buffer(buffer_type buf) {
    delete [] buf;
  }
  
  buffer_type grow (buffer_type old_buf,
                    int64_t old_capacity,
                    int64_t new_capacity,
                    int64_t b,
                    int64_t t) {
    buffer_type new_buf = new_buffer(new_capacity);
    for (int64_t i = t; i < b; i++)
      cb_put (new_buf, new_capacity, i, cb_get (old_buf, old_capacity, i));
    return new_buf;
  }
  
  bool cas_top (int64_t old_val, int64_t new_val) {
    int64_t ov = old_val;
    return top.compare_exchange_strong(ov, new_val);
  }
  
public:
  
  cldeque()
  : buf(nullptr), bottom(0l), top(0l) {
    capacity.store(0l);
  }
  
  void init(int64_t init_capacity) {
    capacity.store(init_capacity);
    buf = new_buffer(capacity.load());
    bottom.store(0l);
    top.store(0l);
    for (int64_t i = 0; i < capacity.load(); i++)
      buf[i].store(nullptr); // optional
  }
  
  void destroy() {
    assert (bottom.load() - top.load() == 0); // maybe wrong
    delete_buffer(buf);
  }
  
  void push_back(value_type item) {
    int64_t b = bottom.load();
    int64_t t = top.load();
    if (b-t >= capacity.load() - 1) {
      buffer_type old_buf = buf;
      int64_t old_capacity = capacity.load();
      int64_t new_capacity = capacity.load() * 2;
      buf = grow (old_buf, old_capacity, new_capacity, b, t);
      capacity.store(new_capacity);
      // UNSAFE! delete old_buf;
    }
    cb_put (buf, capacity.load(), b, item);
    // requires fence store-store
    bottom.store(b + 1);
  }
  
  value_type pop_front(pop_result_type& result) {
    int64_t t = top.load();
    // requires fence load-load
    int64_t b = bottom.load();
    // would need a fence load-load if were to read the pointer on deque->buf
    if (t >= b) {
      result = Pop_failed_with_empty_deque;
      return nullptr;
    }
    value_type item = cb_get (buf, capacity.load(), t);
    // requires fence load-store
    if (! cas_top (t, t + 1)) {
      result = Pop_failed_with_cas_abort;
      return nullptr;
    }
    result = Pop_succeeded;
    return item;
  }
  
  value_type pop_back(pop_result_type& result) {
    int64_t b = bottom.load() - 1;
    bottom.store(b);
    // requires fence store-load
    int64_t t = top.load();
    if (b < t) {
      bottom.store(t);
      result = Pop_failed_with_empty_deque;
      return nullptr;
    }
    value_type item = cb_get (buf, capacity.load(), b);
    if (b > t) {
      result = Pop_succeeded;
      return item;
    }
    if (! cas_top (t, t + 1)) {
      item = (value_type)nullptr;
      result = Pop_failed_with_cas_abort;
    } else {
      result = Pop_succeeded;
    }
    bottom.store(t + 1);
    return item;
  }
  
  size_t size() {
    return (size_t)bottom.load() - top.load();
  }
  
  bool empty() {
    return size() < 1;
  }
  
};

/***********************************************************************/

} // end namespace
} // end namespace

#endif /*! _PASL_DATA_cldeque_H_ */