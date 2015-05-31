/* COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file pstring.hpp
 * \brief Array-based implementation of strings
 *
 */

#include <cmath>

#include "parray.hpp"

#ifndef _PCTL_PSTRING_BASE_H_
#define _PCTL_PSTRING_BASE_H_

namespace pasl {
namespace pctl {
  
/***********************************************************************/

/*---------------------------------------------------------------------*/
/* Parallel string */

class pstring {
private:
  
  using parray_type = parray<char>;
  
public:
  
  using value_type = char;
  using allocator_type = std::allocator<char>;
  using size_type = std::size_t;
  using ptr_diff = std::ptrdiff_t;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  using iterator = typename parray_type::iterator;
  using const_iterator = typename parray_type::const_iterator;
  
  parray_type chars;
  
private:
  
  void make_null_terminated() {
    chars[size()] = '\0';
  }
  
  void init() {
    make_null_terminated();
  }
  
  void check(long i) const {
    assert(i < size());
  }
  
public:
  
  pstring(long sz = 0)
  : chars(sz+1, ' ') {
    make_null_terminated();
  }
  
  pstring(long sz, const value_type& val)
  : chars(sz+1, val) {
    make_null_terminated();
  }
  
  pstring(long sz, const std::function<value_type(long)>& body) {
    auto body2 = [&] (long i) {
      if (i == sz) {
        return '\0';
      } else {
        return body(i);
      }
    };
    chars.tabulate(sz + 1, body2);
    make_null_terminated();
  }
  
  pstring(const char* s)
  : chars((char*)s, (char*)s + strlen(s) + 1) { }
  
  pstring(long sz,
          const std::function<long(long)>& body_comp,
          const std::function<value_type(long)>& body) {
    assert(false);
  }
  
  pstring(long sz,
         const std::function<long(long,long)>& body_comp_rng,
         const std::function<value_type(long)>& body) {
    assert(false);
  }
  
  pstring(std::initializer_list<value_type> xs)
  : chars(xs.size()+1) {
    long i = 0;
    for (auto it = xs.begin(); it != xs.end(); it++) {
      new (&chars[i++]) value_type(*it);
    }
    make_null_terminated();
  }
  
  pstring(const pstring& other)
  : chars(other.chars) { }
  
  pstring(iterator lo, iterator hi) {
    long n = hi - lo;
    chars.resize(n + 1);
    pmem::copy(lo, hi, begin());
  }
  
  pstring& operator=(const pstring& other) {
    chars = other.chars;
    return *this;
  }
  
  pstring& operator=(pstring&& other) {
    chars = std::move(other.chars);
    return *this;
  }
  
  value_type& operator[](long i) {
    check(i);
    return chars[i];
  }
  
  const value_type& operator[](long i) const {
    check(i);
    return chars[i];
  }
  
  long size() const {
    return chars.size() - 1;
  }
  
  long length() const {
    return size();
  }
  
  void swap(pstring& other) {
    chars.swap(other.chars);
  }
  
  void resize(long n, const value_type& val) {
    chars.resize(n+1, val);
    make_null_terminated();
  }
  
  void resize(long n) {
    value_type val;
    resize(n, val);
  }
  
  void clear() {
    resize(0);
  }
  
  iterator begin() const {
    return chars.begin();
  }
  
  const_iterator cbegin() const {
    return chars.cbegin();
  }
  
  iterator end() const {
    return chars.end();
  }
  
  const_iterator cend() const {
    return chars.cend();
  }
  
  pstring& operator+=(const pstring& str) {
    long n1 = size();
    long n2 = str.size();
    long n = n1 + n2;
    chars.tabulate(n + 1, [&] (long i) {
      if (i < n1) {
        return chars[i];
      } else if (i == n) {
        return '\0';
      } else {
        i -= n1;
        return str[i];
      }
    });
    return *this;
  }
  
  pstring operator+(const pstring& rhs) {
    pstring result(*this);;
    result += rhs;
    return result;
  }
  
  const char* c_str() {
    return cbegin();
  }
  
};
  
std::ostream& operator<<(std::ostream& out, const pstring& xs) {
  for (auto it = xs.cbegin(); it != xs.cend(); it++) {
    out << *it;
  }
  return out;
}
  
/***********************************************************************/
  
} // end namespace
} // end namespace

#endif /*! _PCTL_PSTRING_BASE_H_ */
