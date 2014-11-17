/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file string.hpp
 * \brief String algorithms
 *
 */

#include "sparray.hpp"

#ifndef _MINICOURSE_STRING_H_
#define _MINICOURSE_STRING_H_

/***********************************************************************/

using mychar = value_type;
using mystring = sparray;
using const_mystring_ref = const sparray&;

mychar char_compare(mychar x, mychar y) {
  if (x < y)
    return -1l;
  else if (x == y)
    return 0l;
  else
    return 1l;
}

value_type string_compare_seq(const_mystring_ref xs, const_mystring_ref ys) {
  todo();
  return -1l;
}

value_type string_compare(const_mystring_ref xs, const_mystring_ref ys) {
  if (xs.size() < ys.size())
    return -1l * string_compare(ys, xs);
  sparray cs = map_pair([] (mychar x, mychar y) { return char_compare(x, y); }, xs, ys);
  return reduce([&] (mychar a, mychar b) { return (a == 0) ? b : a; }, 0, cs);
}

const value_type open_paren = 1l;
const value_type close_paren = -1l;

value_type c2v(char c) {
  assert(c == '(' || c == ')');
  return (c == '(') ? open_paren : close_paren;
}

char v2c(value_type v) {
  assert(v == open_paren || v == close_paren);
  return (v == open_paren) ? '(' : ')';
}

sparray from_parens(std::string str) {
  long sz = str.size();
  sparray tmp = sparray(sz);
  for (long i = 0; i < sz; i++)
    tmp[i] = c2v(str[i]);
  return tmp;
}

std::string to_parens(const sparray& xs) {
  long sz = xs.size();
  std::string str(sz, 'x');
  for (long i = 0; i < sz; i++)
    str[i] = v2c(xs[i]);
  return str;
}

scan_excl_result every_nesting_depth(const_mystring_ref parens) {
  return scan_excl(plus_fct, 0l, parens);
}

bool is_every_nesting_depth_valid(const sparray& nesting_depths) {
  return reduce(and_fct, [] (value_type x) { return x >= 0; }, true, nesting_depths);
}

bool matching_parens(const_mystring_ref parens) {
  scan_excl_result nesting_depths = every_nesting_depth(parens);
  if (nesting_depths.total != 0)
    return false;
  return is_every_nesting_depth_valid(nesting_depths.partials);
}

bool matching_parens(const std::string& xs) {
  return matching_parens(from_parens(xs));
}

/***********************************************************************/

#endif /*! _MINICOURSE_STRING_H_ */
