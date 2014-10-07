/* COPYRIGHT (c) 2011 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file pcontainer.hpp
 * \brief Resizable container data structures for parallel execution
 *
 */

#include <utility>

#include "native.hpp"
#include "container.hpp"
#include "chunkedseq.hpp"
#include "chunkedbag.hpp"

#ifndef _PASL_PCONTAINER_H_
#define _PASL_PCONTAINER_H_

namespace pasl {
namespace data {
namespace pcontainer {

/***********************************************************************/
  
using namespace pasl::sched;

#ifdef PASL_PCONTAINER_CHUNK_CAPACITY
static const int chunk_capacity = PASL_PCONTAINER_CHUNK_CAPACITY;
#else  
static const int chunk_capacity = 512;
#endif
  
template <class Item>
using deque = chunkedseq::bootstrapped::deque<Item, chunk_capacity>;
  
template <class Item>
using stack = chunkedseq::bootstrapped::stack<Item, chunk_capacity>;

template <class Item>
using bag = chunkedseq::bootstrapped::bagopt<Item, chunk_capacity>;

//--------------------------
// for benchmarking purposes
  
template <class Item>
using ftree_deque = chunkedseq::ftree::deque<Item, chunk_capacity>;

template <class Item>
using ftree_stack = chunkedseq::ftree::stack<Item, chunk_capacity>;

template <class Item>
using ftree_bag = chunkedseq::ftree::bagopt<Item, chunk_capacity>;
  
//--------------------------
  
template <class Container, class Body>
void for_each_segment(const Container& cont, const Body& body) {
  using size_type = typename Container::size_type;
  using iterator_type = typename Container::iterator;
  using segment_type = typename Container::segment_type;
  using input_type = std::pair<iterator_type, iterator_type>;
  auto cutoff = [] (const input_type& in) {
    size_type sz_beg = in.first.size();
    size_type sz_end = in.second.size();
    return sz_end - sz_beg <= size_type(native::loop_cutoff);
  };
  auto split = [&] (input_type& src, input_type& dst) {
    size_type sz_beg = src.first.size();
    size_type sz_end = src.second.size();
    iterator_type mid = cont.begin() + ((sz_beg + sz_end) / 2);
    dst.first = mid;
    dst.second = src.second;
    src.second = mid;
  };
  struct { } dummy;
  using dummy_type = typeof(dummy);
  auto join = [] (dummy_type, dummy_type) { };
  auto _body = [&] (input_type& in, dummy_type& out) {
    iterator_type beg = in.first;
    iterator_type end = in.second;
    cont.for_each_segment(beg, end, body);
  };
  input_type in(cont.begin(), cont.end());
  native::forkjoin(in, dummy, cutoff, split, join, _body);
}

template <class Container, class Body>
void for_each(const Container& cont, const Body& body) {
  using size_type = typename Container::size_type;
  using value_type = typename Container::value_type;
  using segment_type = typename Container::segment_type;
  for_each_segment(cont, [&] (value_type* lo, value_type* hi) {
    for (value_type* p = lo; lo < hi; p++)
      body(*p);
  });
}
  
template <class Item, class Body>
void for_each(const stl::deque_seq<Item>& cont, const Body& body) {
  assert(false);
}

template <class Number, class Container, class Body>
void combine(Number lo, Number hi, Container& dst, const Body& body,
             int cutoff = sched::native::loop_cutoff) {
  auto join = [] (Container& c1, Container& c2) {
    c1.concat(c2);
  };
  native::combine(lo, hi, dst, join, body, cutoff);
}

template <class Container_src, class Pointer>
void transfer_contents_to_array(Container_src& src, Pointer dst) {
  using size_type = typename Container_src::size_type;
  using value_type = typename Container_src::value_type;
  using pointer_type = Pointer;
#ifndef SEQUENTIAL_ELISION
  class input_type {
  public:
    Container_src* first;
    Pointer second;
    input_type() {
      this->first = new Container_src();
      this->second = nullptr;
    }
    input_type(Container_src* first, Pointer second)
    : first(first), second(second) { }
    ~input_type() {
      if (first != nullptr)
        delete first;
    }
  };
  struct { } dummy;
  using dummy_type = typeof(dummy);
  auto cutoff = [] (input_type& f) {
    return f.first->size() <= size_type(native::loop_cutoff);
  };
  auto split = [] (input_type& src, input_type& dst) {
    size_type m = src.first->size() / 2;
    src.first->split(src.first->begin() + m, *dst.first);
    dst.second = src.second + m;
  };
  auto join = [] (dummy_type, dummy_type) { };
  auto body = [&] (input_type& in, dummy_type& out) {
    in.first->popn_back(in.second, in.first->size());
  };
  input_type in(&src, dst);
  native::forkjoin(in, dummy, cutoff, split, join, body);
  in.first = NULL;
#else
  size_type i = 0;
  src.for_each([&] (value_type v) { dst[i++] = v; }); 
#endif
}

template <class Container, class Array>
void transfer_contents_to_array_seq(Container& src, Array& dst) {
  dst.alloc(src.size());
  transfer_contents_to_array(src, dst.data());
}
  
/***********************************************************************/
  
} // end namespace
} // end namespace
} // end namespace

#endif //! _PASL_PCONTAINER_H_
