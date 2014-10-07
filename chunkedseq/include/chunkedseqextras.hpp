/*!
 * \author Umut A. Acar
 * \author Arthur Chargueraud
 * \author Mike Rainey
 * \date 2013-2018
 * \copyright 2014 Umut A. Acar, Arthur Chargueraud, Mike Rainey
 *
 * \brief Extra container operations to be shared by chunked structures
 * \file chunkedseqextras.hpp
 *
 */

#ifndef _PASL_DATA_CHUNKEDSEQEXTRAS_H_
#define _PASL_DATA_CHUNKEDSEQEXTRAS_H_

namespace pasl {
namespace data {
namespace chunkedseq {
namespace extras {

/***********************************************************************/
  
/*---------------------------------------------------------------------*/
/* Various special-purpose forms of the split operation */
  
template <class Container, class size_type>
void split_by_index(Container& c, size_type i, Container& other) {
  using measured_type = typename Container::middle_measured_type;
  using algebra_type = typename Container::middle_algebra_type;
  using size_access = typename Container::size_access;
  using predicate_type = itemsearch::less_than_by_position<measured_type, size_type, size_access>;
  c.check(); other.check();
  size_type size_orig = c.size();
  assert(i >= 0);
  assert(i <= size_orig);
  if (size_orig == 0)
    return;
  if (i == size_orig)
    return;
  if (i == 0) {
    c.swap(other);
    return;
  }
  predicate_type p(i);
  measured_type prefix = algebra_type::identity();
  prefix = c.split_aux(p, prefix, other);
  c.check(); other.check();
  size_type size_cur = c.size();
  size_type size_other = other.size();
  assert(size_other + size_cur == size_orig);
  assert(size_cur == i);
  assert(size_other + i == size_orig);
  assert(size_access::csize(prefix) == i);
}
  
template <class Container, class iterator>
void split_by_iterator(Container& c, iterator position, Container& other) {
  using size_type = typename Container::size_type;
  if (position == c.end())
    return;
  size_type n = position.size() - 1;
  c.split(n, other);
}
  
template <class Container>
void split_approximate(Container& c, Container& other) {
  using size_type = typename Container::size_type;
  assert(c.size() > 1);
  size_type mid = c.size() / 2;
  c.split(mid, other);
}

/*---------------------------------------------------------------------*/
/* Insert and erase */
  
template <class Container, class iterator, class value_type>
iterator insert(Container& c, iterator position, const value_type& val) {
  using self_type = Container;
  using size_type = typename Container::size_type;
  c.check();
  size_type n = position.size() - 1;
  self_type tmp;
  c.split(position, tmp);
  c.push_back(val);
  c.concat(tmp);
  c.check();
  return c.begin() + n;
}

template <class Container, class iterator>
iterator erase(Container& c, iterator first, iterator last) {
  using self_type = Container;
  using size_type = typename Container::size_type;
  if (first == last)
    return first;
  size_type sz_orig = c.size();
  self_type items_to_erase;
  size_type sz_first = first.size();
  size_type sz_last = last.size();
  size_type nb_to_erase = sz_last - sz_first;
  c.split(first, items_to_erase);
  self_type tmp;
  items_to_erase.split(nb_to_erase, tmp);
  items_to_erase.swap(tmp);
  c.concat(items_to_erase);
  assert(c.size() + nb_to_erase == sz_orig);
  return c.begin() + sz_first;
}
  
/*---------------------------------------------------------------------*/
/* For each loops */
  
template <class Body, class iterator>
void for_each_segment(iterator begin, iterator end, const Body& f) {
  using segment_type = typename iterator::segment_type;
  if (begin >= end)
    return;
  segment_type seg_end = end.get_segment();
  for (iterator i = begin; i != end; ) {
    segment_type seg = i.get_segment();
    if (seg.begin == seg_end.begin)
      seg.end = seg_end.middle;
    f(seg.middle, seg.end);
    i += seg.end - seg.middle;
  }
}
  
template <class Body, class iterator>
void for_each(iterator beg, iterator end, const Body& f) {
  using value_type = typename iterator::value_type;
  for_each_segment(beg, end, [&] (value_type* lo, value_type* hi) {
    for (value_type* p = lo; p < hi; p++)
      f(*p);
  });
}
  
/*---------------------------------------------------------------------*/
/* Streaming operations */
  
template <class Container, class Consumer, class size_type>
void stream_backn(const Container& c, const Consumer& cons, size_type nb) {
  using const_pointer = typename Container::const_pointer;
  assert(c.size() >= nb);
  size_type nb_before_target = c.size() - nb;
  c.for_each_segment(c.begin() + nb_before_target, c.end(), [&] (const_pointer lo, const_pointer hi) {
    size_type nb_items_to_copy = size_type(hi - lo);
    cons(lo, nb_items_to_copy);
  });
}

template <class Container, class Consumer, class size_type>
void stream_frontn(const Container& c, const Consumer& cons, size_type nb) {
  using const_pointer = typename Container::const_pointer;
  assert(c.size() >= nb);
  c.for_each_segment(c.begin(), c.begin() + nb, [&] (const_pointer lo, const_pointer hi) {
    size_type nb = size_type(hi - lo);
    cons(lo, nb);
  });
}
  
template <class Container, class value_type, class size_type>
void backn(const Container& c, value_type* dst, size_type nb) {
  using const_pointer = const value_type*;
  using allocator_type = typename Container::allocator_type;
  value_type* p = dst;
  auto cons = [&] (const_pointer lo, size_type nb) {
    fixedcapacity::base::copy<allocator_type>(p, lo, nb);
    p += nb;
  };
  c.stream_backn(cons, nb);
}

template <class Container, class value_type, class size_type>
void frontn(const Container& c, value_type* dst, size_type nb) {
  using const_pointer = const value_type*;
  using allocator_type = typename Container::allocator_type;
  value_type* p = dst;
  auto cons = [&] (const_pointer lo, size_type nb) {
    fixedcapacity::base::copy<allocator_type>(p, lo, nb);
    p += nb;
  };
  c.stream_frontn(cons, nb);
}
  
template <class Container, class const_pointer, class size_type>
void pushn_back(Container& c, const_pointer src, size_type nb) {
  auto prod = [src] (size_type i, size_type nb) {
    const_pointer lo = src + i;
    const_pointer hi = lo + nb;
    return std::make_pair(lo, hi);
  };
  c.stream_pushn_back(prod, nb);
}
  
template <class Container, class const_pointer, class size_type>
void pushn_front(Container& c, const_pointer src, size_type nb) {
  auto prod = [src] (size_type i, size_type nb) {
    const_pointer lo = src + i;
    const_pointer hi = lo + nb;
    return std::make_pair(lo, hi);
  };
  c.stream_pushn_front(prod, nb);
}
  
template <class Container, class value_type, class size_type>
void popn_back(Container& c, value_type* dst, size_type nb) {
  using const_pointer = const value_type*;
  using allocator_type = typename Container::allocator_type;
  value_type* p = dst + nb;
  auto cons = [&] (const_pointer lo, const_pointer hi) {
    size_type d = hi - lo;
    p -= d;
    fixedcapacity::base::copy<allocator_type>(p, lo, d);
  };
  c.template stream_popn_back<typeof(cons), true>(cons, nb);
}

template <class Container, class value_type, class size_type>
void popn_front(Container& c, value_type* dst, size_type nb) {
  using const_pointer = const value_type*;
  using allocator_type = typename Container::allocator_type;
  value_type* p = dst;
  auto cons = [&] (const_pointer lo, const_pointer hi) {
    size_type d = hi - lo;
    fixedcapacity::base::copy<allocator_type>(p, lo, d);
    p += d;
  };
  c.template stream_popn_front<typeof(cons), true>(cons, nb);
}
  
/*---------------------------------------------------------------------*/
/* Debugging output */
  
template <class Container>
std::ostream& generic_print_container(std::ostream& out, const Container& seq) {
  using value_type = typename Container::value_type;
  using size_type = typename Container::size_type;
  size_type sz = seq.size();
  out << "[";
  seq.for_each([&] (value_type x) {
    sz--;
    if (sz == 0)
      out << x;
    else
      out << x << ", ";
  });
  out << "]";
  return out;
}
  
/*---------------------------------------------------------------------*/

  
  /* todo:
   *
   * - assign
   * - at
   * - relational operators http://www.cplusplus.com/reference/vector/vector/operators/
   *
   */

/***********************************************************************/
  
} // end namespace
} // end namespace
} // end namespace
} // end namespace

#endif /*! _PASL_DATA_CHUNKEDSEQEXTRAS_H_ */