/*!
 * \author Umut A. Acar
 * \author Arthur Chargueraud
 * \author Mike Rainey
 * \date 2013-2018
 * \copyright 2014 Umut A. Acar, Arthur Chargueraud, Mike Rainey
 *
 * \brief Representation of a chunk
 * \file bootchunkedseq.hpp
 *
 */

#include <memory>
#include <assert.h>
#include <vector>
#include <iostream>

#include "atomic.hpp"
#include "chunk.hpp"
#include "fixedcapacity.hpp"
#include "cachedmeasure.hpp"
#include "itemsearch.hpp"
#include "annotation.hpp"

#ifndef _PASL_DATA_BOOTCHUNKEDSEQNEW_H_
#define _PASL_DATA_BOOTCHUNKEDSEQNEW_H_

namespace pasl {
namespace data {
namespace chunkedseq {
namespace bootchunkedseq {

/***********************************************************************/
// Pair of an item and a cached measure

template <class Item, class Measured>
class Cached_item {

private:
  Item item;
  Measured cached;

public:
  Cached_item() { }
  Cached_item(Item& item, Measured cached) : item(item), cached(cached) { }
  Cached_item(const Cached_item& other) {
    item = other.item;
    cached = other.cached;
  }

  Measured get_cached() const { return cached; }

  Item get_item() const { return item; }
};


/***********************************************************************/

template <class Top_item_base,  // use "foo" to obtain a sequence of "foo*" items
          int Chunk_capacity = 32,
          class Cached_measure = cachedmeasure::trivial<Top_item_base*, size_t>,
          class Top_item_deleter = Pointer_deleter, // provides: static void dealloc(foo* x)
          class Top_item_copier = Pointer_deep_copier, // provides: static void copy(foo* x)
          template<class Item, int Capacity, class Item_alloc> class Chunk_struct = fixedcapacity::heap_allocated::ringbuffer_ptr,
          class Size_access=itemsearch::no_size_access
          >
class cdeque {

  using self_type = cdeque<Top_item_base, Chunk_capacity, Cached_measure,
                           Top_item_deleter, Top_item_copier, Chunk_struct, Size_access>;

public:

  using size_type = size_t; // TODO: from template argument ?

  using top_cache_type = Cached_measure;
  using top_measured_type = typename top_cache_type::measured_type;
  using top_algebra_type = typename top_cache_type::algebra_type;
  using top_measure_type = typename top_cache_type::measure_type;

private:

  union item_type; // Forward declaration.

  using cached_item_type = Cached_item<item_type, top_measured_type>;

  class cache_type {
  public:

    using size_type = size_t;
    using measured_type = top_measured_type;
    using algebra_type = top_algebra_type;
    class measure_type {
    public:

      measured_type operator()(const cached_item_type& x) const {
        return x.get_cached();
      }

      measured_type operator()(const cached_item_type* lo, const cached_item_type* hi) const {
        measured_type m = algebra_type::identity();
        for (auto p = lo; p < hi; p++)
          m = algebra_type::combine(m, operator()(*p));
        return m;
      }

    };

    static void swap(measured_type& x, measured_type& y) {
      std::swap(x, y);
    }

  };

  using measured_type = typename cache_type::measured_type;
  using algebra_type = typename cache_type::algebra_type;
  using measure_type = typename cache_type::measure_type;

  using queue_type = Chunk_struct<cached_item_type, Chunk_capacity, std::allocator<cached_item_type> >;

  using annotation_type = typename Top_item_base::annotation_type;

  using chunk_type = chunk<queue_type, cache_type, annotation_type>;
  using chunk_pointer = chunk_type*;
  using const_chunk_pointer = const chunk_type*;

  measure_type meas_fct;

  using top_item_type = Top_item_base*;

  union item_type { // 1 word
    top_item_type top_item;
    chunk_pointer rec_item;
    };

  // Representation of the layers:

  /*---------------------------------------------------------------------*/

  class layer {
  public:
    measure_type meas_fct; // todo: get rid of this!

    measured_type cached; // only used for deep layers
    chunk_type front_outer, front_inner, back_inner, back_outer;
    chunk_type& shallow_chunk /*= front_outer */; // shallow layers only use front_outer
    layer* middle;  // null iff shallow layer
      // if the structure is empty, then it must be shallow
      // w.r.t. paper:
      // - we allow deep structures to contain a single item
      // - we enforce that if an outer buffer is empty then the corresponding inner buffer is empty
      // - we enforce that if both outer buffers are empty then the middle is empty
    annotation_type annotation;

    using self_type = layer;

    layer() : shallow_chunk(front_outer) {
      // build a shallow layer representing an empty structure
      middle = NULL;
    }

    ~layer() {
      // recursively deallocate layers
      if (middle != NULL)
        delete middle;
    }

    /* note: our combine operator is not necessarily commutative.
     * as such, we need to be careful to get the order of the
     * operands right when we increment on the front and back.
     */

    void incr_front(measured_type& cached, const measured_type& m) {
      cached = algebra_type::combine(m, cached);
    }

    void incr_back(measured_type& cached, const measured_type& m) {
      cached = algebra_type::combine(cached, m);
    }

    void decr_front(measured_type& cached, const measured_type& m) {
      incr_front(cached, algebra_type::inverse(m));
    }

    void decr_back(measured_type& cached, const measured_type& m) {
      incr_back(cached, algebra_type::inverse(m));
    }

    void rec_copy(int depth, const self_type& other) {
      // alternative: replace the next 2 lines with "this.~layer()".
      if (middle != NULL)
        delete middle;
      cached = other.cached;
      chunk_deep_copy(depth, other.front_outer, front_outer);
      chunk_deep_copy(depth, other.front_inner, front_inner);
      chunk_deep_copy(depth, other.back_inner, back_inner);
      chunk_deep_copy(depth, other.back_outer, back_outer);
      if (other.middle == NULL) { // other is shallow
        middle = NULL;
      } else { // other is deep
        middle = new layer();
        middle->rec_copy(depth+1, *(other.middle));
      }
    }

    void swap(self_type& other) {
      std::swap(cached, other.cached);
      std::swap(middle, other.middle);
      front_outer.swap(other.front_outer);
      front_inner.swap(other.front_inner);
      back_inner.swap(other.back_inner);
      back_outer.swap(other.back_outer);
    }

    void convert_deep_to_shallow() {
      delete middle;
      middle = NULL;
    }

    inline bool is_shallow() const {
      return (middle == NULL);
    }

    void reset_cached() {
      if (is_shallow())
        return;
      cached = algebra_type::identity();
      incr_back(cached, front_outer.get_cached());
      incr_back(cached, front_inner.get_cached());
      incr_back(cached, middle->get_cached());
      incr_back(cached, back_inner.get_cached());
      incr_back(cached, back_outer.get_cached());
    }

    // assumes deep layer;
    // take a chunk "c" and push it into the front of the middle sequence,
    // leaving "c" empty
    inline void push_buffer_front_force(chunk_type& c) {
      chunk_pointer d = chunk_alloc();
      c.swap(*d);
      middle->push_front(cached_item_of_chunk_pointer(d));
    }

    // symmetric to push_buffer_front
    inline void push_buffer_back_force(chunk_type& c) {
      chunk_pointer d = chunk_alloc();
      c.swap(*d);
      middle->push_back(cached_item_of_chunk_pointer(d));
    }

    inline bool empty() const {
      if (is_shallow())
        return shallow_chunk.empty();
      else
        return front_outer.empty() && back_outer.empty();
    }

    inline measured_type get_cached() const {
      if (is_shallow())
        return shallow_chunk.get_cached();
      else
        return cached;
    }

    void push_front(cached_item_type x) {
      if (is_shallow()) {
        if (! shallow_chunk.full()) {
          shallow_chunk.push_front(meas_fct, x);
        } else {
          // from shallow to deep
          middle = new layer();
          cached = shallow_chunk.get_cached();
          shallow_chunk.swap(back_outer);
          // todo: share the next 2 lines with the same lines below
          incr_front(cached, x.get_cached());
          front_outer.push_front(meas_fct, x);
        }
      } else { // deep
        if (front_outer.full()) {
          if (front_inner.full())
            push_buffer_front_force(front_inner);
          front_outer.swap(front_inner);
          assert(front_outer.empty());
        }
        incr_front(cached, x.get_cached());
        front_outer.push_front(meas_fct, x);
      }
    }

    void push_back(cached_item_type x) {
      if (is_shallow()) {
        if (! shallow_chunk.full()) {
          shallow_chunk.push_back(meas_fct, x);
        } else {
          // from shallow to deep
          middle = new layer();
          cached = shallow_chunk.get_cached();
          // noop: shallow_chunk.swap(front_inner);
          incr_back(cached, x.get_cached());
          back_outer.push_back(meas_fct, x);
        }
      } else { // deep
        if (back_outer.full()) {
          if (back_inner.full())
            push_buffer_back_force(back_inner);
          back_outer.swap(back_inner);
          assert(back_outer.empty());
        }
        incr_back(cached, x.get_cached());
        back_outer.push_back(meas_fct, x);
      }
    }

    cached_item_type& front() const {
      if (is_shallow()) {
        return shallow_chunk.front();
      } else { // deep
        assert(! front_outer.empty() || front_inner.empty());
        if (! front_outer.empty()) {
          return front_outer.front();
        } else if (! middle->empty()) {
          chunk_pointer c = chunk_pointer_of_cached_item(middle->front());
          return c->front();
        } else if (! back_inner.empty()) {
          return back_inner.front();
        } else {
          assert(! back_outer.empty());
          return back_outer.front();
        }
      }
    }

    cached_item_type& back() const {
      if (is_shallow()) {
        return shallow_chunk.back();
      } else { // deep
        assert(! back_outer.empty() || back_inner.empty());
        if (! back_outer.empty()) {
          return back_outer.back();
        } else if (! middle->empty()) {
          chunk_pointer c = chunk_pointer_of_cached_item(middle->back());
          return c->back();
        } else if (! front_inner.empty()) {
          return front_inner.back();
        } else {
          assert(! front_outer.empty());
          return front_outer.back();
        }
      }
    }

    // assumes a deep structure;
    // ensures that if the structure is not empty then front_outer is not empty,
    // and if the structure is empty then it becomes shallow.
    // remark: acting on the back chunks is not needed for invariants, but helps for efficiency in typical application cases.
    void try_populate_front_outer() {
      if (front_outer.empty()) {
        if (! front_inner.empty()) {
          front_inner.swap(front_outer);
        } else if (! middle->empty()) {
          chunk_pointer c = chunk_pointer_of_cached_item(middle->pop_front());
          front_outer.swap(*c);
          chunk_free_when_empty(c);
        } else if (! back_inner.empty()) {
          back_inner.swap(front_outer);
        } else if (! back_outer.empty()) {
          back_outer.swap(front_outer);
        } else { // empty structure
          convert_deep_to_shallow();
        }
      }
    }

    void try_populate_back_outer() {
      if (back_outer.empty()) {
        if (! back_inner.empty()) {
          back_inner.swap(back_outer);
        } else if (! middle->empty()) {
          chunk_pointer c = chunk_pointer_of_cached_item(middle->pop_back());
          back_outer.swap(*c);
          chunk_free_when_empty(c);
        } else if (! front_inner.empty()) {
          front_inner.swap(back_outer);
        } else if (! front_outer.empty()) {
          front_outer.swap(back_outer);
        } else { // empty structure
          convert_deep_to_shallow();
        }
      }
    }

    cached_item_type pop_front() {
      if (is_shallow()) {
        return shallow_chunk.pop_front(meas_fct);
      } else { // deep
        if (front_outer.empty()) {
          assert(front_inner.empty());
          if (! middle->empty()) {
            chunk_pointer c = chunk_pointer_of_cached_item(middle->pop_front());
            front_outer.swap(*c);
            chunk_free_when_empty(c);
          } else if (! back_inner.empty()) {
            back_inner.swap(front_outer);
          } else if (! back_outer.empty()) {
            back_outer.swap(front_outer);
          }
        }
        assert(! front_outer.empty());
        cached_item_type x = front_outer.pop_front(meas_fct);
        if (algebra_type::has_inverse)
          decr_front(cached, x.get_cached());
        else
          reset_cached();
        try_populate_front_outer();
        return x;
      }
    }

    cached_item_type pop_back() {
      if (is_shallow()) {
        return shallow_chunk.pop_back(meas_fct);
      } else { // deep
        if (back_outer.empty()) {
          assert(back_inner.empty());
          if (! middle->empty()) {
            chunk_pointer c = chunk_pointer_of_cached_item(middle->pop_back());
            back_outer.swap(*c);
            chunk_free_when_empty(c);
          } else if (! front_inner.empty()) {
            front_inner.swap(back_outer);
          } else if (! front_outer.empty()) {
            front_outer.swap(back_outer);
          }
        }
        assert(! back_outer.empty());
        cached_item_type x = back_outer.pop_back(meas_fct);
        if (algebra_type::has_inverse)
          decr_back(cached, x.get_cached());
        else
          reset_cached();
        try_populate_back_outer();
        return x;
      }
    }

    // assumption: invariant "both outer empty implies middle empty" may be broken;
    // calling this function restores it; or turns level into shallow if all empty
    void restore_both_outer_empty_middle_empty() {
      if (is_shallow())
        return;
      if (front_outer.empty() && back_outer.empty()) {
        if (middle->empty()) {
          convert_deep_to_shallow();
        } else {
          // pop to the front (to the back would also work)
          chunk_pointer c = chunk_pointer_of_cached_item(middle->pop_front());
          front_outer.swap(*c);
          chunk_free_when_empty(c);
        }
      }
    }

    void ensure_empty_inner() {
      if (! front_inner.empty())
        push_buffer_front_force(front_inner);
      if (! back_inner.empty())
        push_buffer_back_force(back_inner);
    }

	  using position_type = enum {
		 found_front_outer,
		 found_front_inner,
		 found_middle,
		 found_back_inner,
		 found_back_outer,
		 found_nowhere
	  };

	  template <class Pred>
	  measured_type search_in_layer(const Pred& p, measured_type prefix, position_type& pos) const {
      measured_type cur = prefix; // prefix including current chunk
      // common code for shallow and deep
      if (! front_outer.empty()) {
        prefix = cur;
        cur = algebra_type::combine(cur, front_outer.get_cached());
        if (p(cur)) {
          pos = found_front_outer;
          return prefix;
        }
      }
      // special case for shallow
      {
        if (is_shallow()) {
          prefix = cur;
          pos = found_nowhere;
          return prefix;
        }
      }
      if (! front_inner.empty()) {
        prefix = cur;
        cur = algebra_type::combine(cur, front_inner.get_cached());
        if (p(cur)) {
          pos = found_front_inner;
          return prefix;
        }
      }
      if (! middle->empty()) {
        prefix = cur;
        cur = algebra_type::combine(prefix, middle->get_cached());
        if (p(cur)) {
          pos = found_middle;
          return prefix;
        }
      }
      if (! back_inner.empty()) {
        prefix = cur;
        cur = algebra_type::combine(cur, back_inner.get_cached());
        if (p(cur)) {
          pos = found_back_inner;
          return prefix;
        }
      }
      if (! back_outer.empty()) {
        prefix = cur;
        cur = algebra_type::combine(cur, back_outer.get_cached());
        if (p(cur)) {
          pos = found_back_outer;
          return prefix;
        }
      }
      prefix = cur;
      pos = found_nowhere;
      return prefix;
	  }

    measure_type get_measure() const {
      return meas_fct;
    }

    template <class Node, class Pointer>
    static void cache_search_data_for_backtracking(const Node& nd,
                                                   Pointer ptr,
                                                   const annotation::parent_pointer_tag tag,
                                                   const int depth,
                                                   const measured_type prefix) {
      nd.annotation.parent.set_pointer(ptr, tag);
      assert(!annotation_type::finger_search_enabled ||
             nd.annotation.parent.template get_tag() == tag);
      nd.annotation.parent.set_depth(depth);
      nd.annotation.parent.template set_prefix<measured_type>(prefix);
    }

    template <class Pred>
    static measured_type chunk_search(const measure_type& meas_fct,
                                      const chunk_type& c,
                                      const int depth,
                                      const Pred& p,
                                      const measured_type prefix,
                                      const Top_item_base*& r) {
      chunk_search_type search;
      chunk_search_result_type s = search(c, meas_fct, prefix, p);
      measured_type prefix_res = s.prefix;
      size_type i = s.position - 1;
      cached_item_type& t = c[i];
      const annotation::parent_pointer_tag tag = annotation::PARENT_POINTER_BOOTSTRAP_INTERIOR_NODE;
      if (depth == 0) {
        r = top_item_of_cached_item(t);
        cache_search_data_for_backtracking(*r, &c, tag, depth, prefix);
      } else {
        const_chunk_pointer tc = const_chunk_pointer_of_cached_item(t);
        cache_search_data_for_backtracking(*tc, &c, tag, depth, prefix);
        prefix_res = chunk_search(meas_fct, *tc, depth-1, p, prefix_res, r);
      }
      return prefix_res;
    }

    template <class Pred>
    measured_type rec_search(const measure_type& meas_fct, const int depth,
                             const Pred& p, const measured_type prefix,
                             const Top_item_base*& r) const {
      position_type pos;
      measured_type prefix_res = search_in_layer(p, prefix, pos);
      const annotation::parent_pointer_tag tag = annotation::PARENT_POINTER_BOOTSTRAP_LAYER_NODE;
      switch (pos) {
        case found_front_outer: {
          cache_search_data_for_backtracking(front_outer, this, tag, depth, prefix);
          prefix_res = chunk_search(meas_fct, front_outer, depth, p, prefix_res, r);
          break;
        }
        case found_front_inner: {
          cache_search_data_for_backtracking(front_inner, this, tag, depth, prefix);
          prefix_res = chunk_search(meas_fct, front_inner, depth, p, prefix_res, r);
          break;
        }
        case found_middle: {
          cache_search_data_for_backtracking(*middle, this, tag, depth, prefix);
          prefix_res = middle->rec_search(meas_fct, depth+1, p, prefix_res, r);
          break;
        }
        case found_back_inner: {
          cache_search_data_for_backtracking(back_inner, this, tag, depth, prefix);
          prefix_res = chunk_search(meas_fct, back_inner, depth, p, prefix_res, r);
          break;
        }
        case found_back_outer: {
          cache_search_data_for_backtracking(back_outer, this, tag, depth, prefix);
          prefix_res = chunk_search(meas_fct, back_outer, depth, p, prefix_res, r);
          break;
        }
        case found_nowhere: {
          assert(false);
          break;
        }
      } // end switch
      return prefix_res;
    }

    template <class Pred>
    measured_type backtrack_search(const Pred& p, const measured_type prefix,
                                   const Top_item_base*& r) const {
      using tree_pointer_type = union {
        const self_type* layer;
        const_chunk_pointer interior_node;
        const Top_item_base* chunk;
      };
      auto get_parent_linkage = [] (annotation::parent_pointer_tag tag_cur,
                                    tree_pointer_type ptr_cur,
                                    annotation::parent_pointer_tag& tag_next,
                                    tree_pointer_type& ptr_next,
                                    int& depth_next,
                                    measured_type& prefix_next) {
        switch (tag_cur) {
          case annotation::PARENT_POINTER_BOOTSTRAP_INTERIOR_NODE: {
            const_chunk_pointer nd = ptr_cur.interior_node;
            tag_next = nd->annotation.parent.get_tag();
            ptr_next.interior_node = nd->annotation.parent.template get_pointer<const_chunk_pointer>();
            depth_next = nd->annotation.parent.get_depth();
            prefix_next = nd->annotation.parent.template get_prefix<measured_type>();
            break;
          }
          case annotation::PARENT_POINTER_BOOTSTRAP_LAYER_NODE: {
            const self_type* layer = ptr_cur.layer;
            tag_next = layer->annotation.parent.get_tag();
            ptr_next.layer = layer->annotation.parent.template get_pointer<const self_type*>();
            depth_next = layer->annotation.parent.get_depth();
            prefix_next = layer->annotation.parent.template get_prefix<measured_type>();
            break;
          }
          case annotation::PARENT_POINTER_CHUNK: {
            const Top_item_base* chunk = ptr_cur.chunk;
            tag_next = chunk->annotation.parent.get_tag();
            ptr_next.interior_node = chunk->annotation.parent.template get_pointer<const_chunk_pointer>();
            depth_next = chunk->annotation.parent.get_depth();
            prefix_next = chunk->annotation.parent.template get_prefix<measured_type>();
            break;
          }
          case annotation::PARENT_POINTER_UNINITIALIZED:
          default: {
            assert(false);
            tag_next = annotation::PARENT_POINTER_UNINITIALIZED;
            ptr_next.interior_node = nullptr;
            depth_next = -1;
            prefix_next = algebra_type::identity();
            break;
          }
        } // end switch
      };
      assert(r != nullptr);
      top_measure_type top_meas_fct;
      measured_type prefix_cur = r->annotation.prefix.template get_cached<measured_type>();
      annotation::parent_pointer_tag tag_cur = annotation::PARENT_POINTER_CHUNK;
      tree_pointer_type ptr_cur;
      ptr_cur.chunk = r;
      int depth_cur = r->annotation.parent.get_depth() - 1;
      while (true) {
        auto finished_backtracking = [&] (measured_type m) {
          measured_type n = algebra_type::combine(prefix_cur, m);
          return !p(prefix_cur) && p(n);
        };

        switch (tag_cur) {
          case annotation::PARENT_POINTER_BOOTSTRAP_INTERIOR_NODE: {
            const_chunk_pointer nd = ptr_cur.interior_node;
            if (finished_backtracking(nd->get_cached())) {
              return chunk_search(meas_fct, *nd, depth_cur, p, prefix_cur, r);
            }
            break;
          }
          case annotation::PARENT_POINTER_BOOTSTRAP_LAYER_NODE: {
            const self_type* layer = ptr_cur.layer;
            if (finished_backtracking(layer->get_cached())) {
              return layer->rec_search(meas_fct, depth_cur, p, prefix_cur, r);
            }
            break;
          }
          case annotation::PARENT_POINTER_CHUNK: {
            const Top_item_base* chunk = ptr_cur.chunk;
            top_measure_type top_meas_fct;
            if (finished_backtracking(top_meas_fct(chunk))) {
              r = chunk;
              return prefix_cur;
            }
            break;
          }
          case annotation::PARENT_POINTER_UNINITIALIZED: {
            return rec_search(meas_fct, depth0, p, prefix, r);
          }
          default: {
            assert(false);
            r = nullptr;
            return prefix_cur;
          }
        }
        get_parent_linkage(tag_cur, ptr_cur, tag_cur, ptr_cur, depth_cur, prefix_cur);
      }
      assert(false);
      r = nullptr;
      return prefix_cur;
    }

    template <class Pred>
    measured_type search(const Pred& p, measured_type prefix, const Top_item_base*& r) const {
      measure_type meas_fct = get_measure();
      if (annotation_type::finger_search_enabled && r != nullptr)
        return backtrack_search(p, prefix, r);
      else
        return rec_search(meas_fct, depth0, p, prefix, r);
    }

	  // precondition: other is empty (and therefore shallow)
	  template <class Pred>
	  measured_type split(const Pred& p, measured_type prefix, cached_item_type& x, self_type& other) {
      assert(other.empty() && other.is_shallow());
      // TODO: needed?  --equivalent to: copy_measure_to(other);
      //   other.meas_fct = meas_fct;
      if (is_shallow()) {
        // if (shallow_chunk.empty())   TODO?
        //  return prefix;
        prefix = chunk_split(p, prefix, shallow_chunk, x, other.shallow_chunk);
      } else { // deep
        other.middle = new layer(); // turn other from shallow to deep (might be undone)
        // other.cached will be reset later
        ensure_empty_inner();
        position_type pos;
        prefix = search_in_layer(p, prefix, pos);
        switch (pos) {
        case found_front_outer: {
          prefix = chunk_split(p, prefix, front_outer, x, other.front_outer);
          std::swap(middle, other.middle);
          back_outer.swap(other.back_outer);
          break;
        }
        case found_front_inner: {
          assert(false); // thanks to the call to ensure_empty_inner()
          break;
        }
        case found_middle: {
          back_outer.swap(other.back_outer);
          cached_item_type y;
          prefix = middle->split(p, prefix, y, *(other.middle));
          // TODO: use a function to factorize next 3 lines with other places
          chunk_pointer c = chunk_pointer_of_cached_item(y);
          back_outer.swap(*c);
          chunk_free_when_empty(c);
          prefix = chunk_split(p, prefix, back_outer, x, other.front_outer);
          break;
        }
        case found_back_inner: {
          assert(false); // thanks to the call to ensure_empty_inner()
          break;
        }
        case found_back_outer: {
          prefix = chunk_split(p, prefix, back_outer, x, other.back_outer);
          break;
        }
        case found_nowhere: {
          // don't split (item not found)
          break;
        }
        } // end switch
        // reset cached values
        reset_cached();
        other.reset_cached();
        // restore invariants
        restore_both_outer_empty_middle_empty();
        other.restore_both_outer_empty_middle_empty();
      }
      return prefix;
	  }

	  // take a chunk "c" and concatenate its content into the back of the middle sequence
	  // leaving "c" empty.
    // assumes deep level
	  void push_buffer_back(chunk_type& c) {
		 size_t csize = c.size();
		 if (csize == 0) {
			// do nothing
		 } else if (middle->empty()) {
			push_buffer_back_force(c);
		 } else {
			chunk_pointer b = chunk_pointer_of_cached_item(middle->back());
			size_t bsize = b->size();
			if (bsize + csize > Chunk_capacity) {
			  push_buffer_back_force(c);
			} else {
			  middle->pop_back();
			  c.transfer_from_front_to_back(meas_fct, *b, csize);
			  middle->push_back(cached_item_of_chunk_pointer(b));
			}
		 }
	  }

	  // symmetric to push_buffer_back
	  void push_buffer_front(chunk_type& c) {
		 size_t csize = c.size();
		 if (csize == 0) {
			// do nothing
		 } else if (middle->empty()) {
			push_buffer_front_force(c);
		 } else {
			chunk_pointer b = chunk_pointer_of_cached_item(middle->front());
			size_t bsize = b->size();
			if (bsize + csize > Chunk_capacity) {
			  push_buffer_front_force(c);
			} else {
			  middle->pop_front();
			  c.transfer_from_back_to_front(meas_fct, *b, csize);
			  middle->push_front(cached_item_of_chunk_pointer(b));
			}
		 }
	  }

	  // concatenate data from other; leaving other empty
	  void concat(self_type& other) {
      if (other.is_shallow()) {
        size_type nb = other.shallow_chunk.size();
        for (int i = 0; i < nb; i++) {
          cached_item_type x = other.shallow_chunk.pop_front(meas_fct);
          push_back(x);
        }
      } else if (is_shallow()) {
        swap(other);
        size_type nb = other.shallow_chunk.size();
        for (int i = 0; i < nb; i++) {
          cached_item_type x = other.shallow_chunk.pop_back(meas_fct);
          push_front(x);
        }
      } else { // both deep
        // push buffers into the middle sequences
        push_buffer_back(back_inner);
        push_buffer_back(back_outer);
        other.push_buffer_front(other.front_inner);
        other.push_buffer_front(other.front_outer);
        // fuse front and back, if needed
        if (! middle->empty() && ! other.middle->empty()) {
          chunk_pointer c1 = chunk_pointer_of_cached_item(middle->back());
          chunk_pointer c2 = chunk_pointer_of_cached_item(other.middle->front());
          size_type nb1 = c1->size();
          size_type nb2 = c2->size();
          if (nb1 + nb2 <= Chunk_capacity) {
            middle->pop_back();
            other.middle->pop_front();
            c2->transfer_from_front_to_back(meas_fct, *c1, nb2);
            chunk_free_when_empty(c2);
            middle->push_back(cached_item_of_chunk_pointer(c1));
          }
        }
        // migrate back chunks of the other
        back_inner.swap(other.back_inner);
        back_outer.swap(other.back_outer);
        // concatenate the middle sequences
        middle->concat(*(other.middle));
        // update the cache --alternative: reset_cached();
        cached = algebra_type::combine(cached, other.cached);
        // restore invariants
        restore_both_outer_empty_middle_empty();
        // turn other (which is now empty) into shallow
        other.convert_deep_to_shallow();
        assert(other.empty());
        }
    }

	  void rec_check(int depth) const {
		 #ifdef BOOTCHUNKEDSEQ_CHECK
		 if (! is_shallow()) { // deep
			measured_type wfo = chunk_check_weight(depth, front_outer);
			measured_type wfi = chunk_check_weight(depth, front_inner);
			measured_type wbi = chunk_check_weight(depth, back_inner);
			measured_type wbo = chunk_check_weight(depth, back_outer);
			measured_type wmid = middle->get_cached();
			measured_type w = algebra_type::combine(wfo, wfi);
			w = algebra_type::combine(w, wmid);
			w = algebra_type::combine(w, wbi);
			w = algebra_type::combine(w, wbo);
			size_t sfo = front_outer.size();
			size_t sfi = front_inner.size();
			size_t sbi = back_inner.size();
			size_t sbo = back_outer.size();
			assert (sfo != 0 || sfi == 0); // outer empty implies inner empty
			assert (sbo != 0 || sbi == 0); // outer empty implies inner empty
      assert (sfi == 0 || sfi == Chunk_capacity); // empty or full
			assert (sbi == 0 || sbi == Chunk_capacity); // empty or full
			assert (sfo + sbo > 0); // not both outer empty (else shallow)
      // todo: invariant on chunks in middle sequence
			middle->rec_check(depth+1);
		 }
		 #endif
	  }

	  void rec_print(int depth)  {
		 if (is_shallow()) {
			printf("S:");
			rec_print_chunk(depth, shallow_chunk);
			printf("\n");
		 } else { // deep
			printf("D:{");
			// CachePrinter::print(d.cached);
			std::cout << cached;
			printf("} ");
			rec_print_chunk(depth, front_outer);
			printf("  ");
			rec_print_chunk(depth, front_inner);
			printf(" || ");
			rec_print_chunk(depth, back_inner);
			printf("  ");
			rec_print_chunk(depth, back_outer);
			printf("\n");
			middle->rec_print(depth+1);
		 }
	  }

    template <class Add_edge, class Process_chunk>
    void rec_reveal_internal_structure(const Add_edge& add_edge,
                                       const Process_chunk& process_chunk,
                                       int depth) const {
      if (is_shallow()) {
        add_edge(this, &shallow_chunk);
        rec_reveal_internal_structure_of_chunk(add_edge, process_chunk, depth, shallow_chunk);
      } else { // deep
        add_edge((void*)this, (void*)&front_outer);
        add_edge((void*)this, (void*)&front_inner);
        add_edge((void*)this, (void*)middle);
        add_edge((void*)this, (void*)&back_inner);
        add_edge((void*)this, (void*)&back_outer);
        rec_reveal_internal_structure_of_chunk(add_edge, process_chunk, depth, front_outer);
        rec_reveal_internal_structure_of_chunk(add_edge, process_chunk, depth, front_inner);
        middle->rec_reveal_internal_structure(add_edge, process_chunk, depth+1);
        rec_reveal_internal_structure_of_chunk(add_edge, process_chunk, depth, back_inner);
        rec_reveal_internal_structure_of_chunk(add_edge, process_chunk, depth, back_outer);
      }
    }

    template <class Body>
    void rec_for_each(int depth, const Body& f) const {
      if (is_shallow()) {
        chunk_for_each(depth, f, shallow_chunk);
      } else {
        chunk_for_each(depth, f, front_outer);
        chunk_for_each(depth, f, front_inner);
        middle->rec_for_each(depth+1, f);
        chunk_for_each(depth, f, back_inner);
        chunk_for_each(depth, f, back_outer);
      }
    }
  };

  /*---------------------------------------------------------------------*/

  layer top_layer;

  /*---------------------------------------------------------------------*/

  static inline cached_item_type cached_item_of_chunk_pointer(chunk_pointer c) {
    item_type x;
    x.rec_item = c;
    measured_type w = c->get_cached();
    cached_item_type v(x, w);
    return v;
  }

  static inline cached_item_type cached_item_of_top_item(top_item_type y, measured_type w) {
    item_type x;
    x.top_item = y;
    cached_item_type v(x, w);
    return v;
  }

  static inline chunk_pointer chunk_pointer_of_cached_item(const cached_item_type v) {
    return v.get_item().rec_item;
  }

  static inline const_chunk_pointer const_chunk_pointer_of_cached_item(const cached_item_type v) {
    return v.get_item().rec_item;
  }

  static inline top_item_type top_item_of_cached_item(const cached_item_type v) {
    return v.get_item().top_item;
  }


  /*---------------------------------------------------------------------*/

  static inline chunk_pointer chunk_alloc() {
    return new chunk_type();
  }

  static inline void chunk_free_when_empty(chunk_pointer c) {
    delete c;
  }

  // recursively delete all the objects stored in the chunk,
  // leaving the current chunk in an unstable state;
  // only use this function to implement the destructor
  static inline void chunk_deep_free(int depth, chunk_type& c) {
    c.for_each([depth] (cached_item_type& v) {
      if (depth == 0) {
        top_item_type x = top_item_of_cached_item(v);
        Top_item_deleter::dealloc(x);
      } else {
        chunk_pointer d = chunk_pointer_of_cached_item(v);
        chunk_deep_free(depth-1, *d);
        delete d;
      }
    });
  }

  static inline void chunk_deep_free_and_destruct(int depth, chunk_type& c) {
    chunk_deep_free(depth, c);
    c.~chunk_type();
  }

  // recursively copy a chunk into another one,
  // using the copy constructor of class Top_item_base for copying top items
  static void chunk_deep_copy(int depth, const chunk_type& src, chunk_type& dst) {
    src.for_each([&] (const cached_item_type& v) {
      cached_item_type c;
      if (depth == 0) {
        top_item_type orig = top_item_of_cached_item(v);
        top_item_type copy = Top_item_copier::copy(orig);
        c = cached_item_of_top_item(copy, v.get_cached());
      } else {
        const_chunk_pointer orig = const_chunk_pointer_of_cached_item(v);
        chunk_pointer copy = new chunk_type();
        chunk_deep_copy(depth-1, *orig, *copy);
        c = cached_item_of_chunk_pointer(copy);
      }
      measure_type meas_fct; //TODO
      dst.push_back(meas_fct, c);
    });
  }

  // apply a given function f to the top items of the tree rooted at node c
  template <class Body>
  static void chunk_for_each(int depth, const Body& f, const chunk_type& c) {
    c.for_each([&] (const cached_item_type& v) {
      if (depth == 0) {
        top_item_type item = top_item_of_cached_item(v);
        f(item);
      } else {
        const_chunk_pointer c = const_chunk_pointer_of_cached_item(v);
        chunk_for_each(depth-1, f, *c);
      }
    });
  }

  static inline measured_type chunk_check_weight(int depth, const chunk_type& c) {
     measured_type wref = c.get_cached();
     measured_type w = algebra_type::identity();
     c.for_each([&w, depth] (cached_item_type& v) {
      if (depth == 0) {
        w = algebra_type::combine(w, v.get_cached());
      } else {
        measured_type wiref = v.get_cached();
        chunk_pointer d = chunk_pointer_of_cached_item(v);
        measured_type wi = chunk_check_weight(depth-1, *d);
        //assert(Size_access::csize(wi) == Size_access::csize(wiref));
        //assert(wi == wiref);
        //TODO?
       w = algebra_type::combine(w, wiref);
      }
     });
     // assert(Size_access::csize(w) == Size_access::csize(wref));
    // assert(w == wref);
    //TODO?
     return wref;
  }

  static void rec_print_item(int depth, cached_item_type& x)  {
     if (depth == 0) {
      top_item_type y = top_item_of_cached_item(x);
      Top_item_base v = *y;
      std::cout << v.value;
     } else {
      chunk_pointer d = chunk_pointer_of_cached_item(x);
      rec_print_chunk(depth-1, *d);
     }
  }

  template <class Add_edge, class Process_chunk>
  static void rec_reveal_internal_structure_of_chunk(const Add_edge& add_edge,
                                              const Process_chunk& process_chunk,
                                              int depth,
                                              const chunk_type& c) {
    c.for_each([&] (const cached_item_type& v) {
      if (depth == 0) {
        top_item_type item = top_item_of_cached_item(v);
        add_edge((void*)&c, (void*)item);
        process_chunk(item);
      } else {
        const_chunk_pointer d = const_chunk_pointer_of_cached_item(v);
        add_edge((void*)&c, (void*)d);
        rec_reveal_internal_structure_of_chunk(add_edge, process_chunk, depth-1, *d);
      }
    });
  }

  static void rec_print_chunk(int depth, const chunk_type& c)  {
     printf("<");
     std::cout << c.get_cached();
     printf(">(");
     c.for_each([&] (cached_item_type& x) {
      rec_print_item(depth, x);
      printf(" ");
     });
     printf(")");
  }

  /*---------------------------------------------------------------------*/

  /* 3-way split: place in `x` the item that reaches the targeted measure.
   *
   * We define the targeted measure as the value `m` for which `p(m)` returns
   * `true`, where `m` equals the combined measures of the items in the
   * sequence up to and including `x`
   * (i.e., `algebra_type::combine(prefix, meas_fct(x))`).
   *
   * Assumes `dst` to be initially uninitialized.
   */
  template <class Pred>
  static measured_type chunk_split(const Pred& p,
                            measured_type prefix,
                            chunk_type& src,
                            cached_item_type& x,
                            chunk_type& dst) {
    measure_type meas_fct; //TODO
    return src.split(meas_fct, p, prefix, x, dst);
  }

  /*---------------------------------------------------------------------*/

  using chunk_search_type = itemsearch::search_in_chunk<chunk_type, algebra_type>;
  using chunk_search_result_type = typename chunk_search_type::result_type;

  /* Find how to split a chunk to reach a measure targeted by a given predicate `p`.
   * Returns the position expressed as a one-based index that is relative to the
   * chunk; and returns the measure, relative to the enclosing sequence, to the
   * left of the position (i.e., the `prefix` value).
   */
  template <class Pred>
  static chunk_search_result_type chunk_search(const Pred& p,
                                        measured_type prefix,
                                        chunk_type& c) {
    chunk_search_type search;
    measure_type meas_fct; //TODO
    return search(c, meas_fct, prefix, p);
  }



  /*---------------------------------------------------------------------*/

  static constexpr int depth0 = 0;

public:

  using value_type = top_item_type;

  cdeque() {}

  ~cdeque() {}

  cdeque(const self_type& other) {
    top_layer.rec_copy(depth0, other.top_layer);
  }

  void swap(self_type& other) {
    top_layer.swap(other.top_layer);
  }

  inline bool empty() const {
    return top_layer.empty();
  }

  inline measured_type get_cached() const {
    return top_layer.get_cached();
  }

  inline void push_front(const top_measure_type& top_meas, const value_type& x) {
    cached_item_type v = cached_item_of_top_item(x, top_meas(x));
    top_layer.push_front(v);
    check();
  }

  inline void push_back(const top_measure_type& top_meas, const value_type& x) {
    cached_item_type v = cached_item_of_top_item(x, top_meas(x));
    top_layer.push_back(v);
    check();
  }

  inline value_type front() const {
    cached_item_type v = top_layer.front();
    return top_item_of_cached_item(v);
  }

  inline value_type back() const {
    cached_item_type v = top_layer.back();
    return top_item_of_cached_item(v);
  }

  inline value_type cback() const {
    return back();
  }


  // TODO: why this unnamed argument?
  inline value_type pop_front(const top_measure_type&) {
    cached_item_type v = top_layer.pop_front();
    return top_item_of_cached_item(v);
    check();
  }

  inline value_type pop_back(const top_measure_type&) {
    cached_item_type v = top_layer.pop_back();
    return top_item_of_cached_item(v);
    check();
  }

  // concatenate the items of "other" to the right of the current sequence,
  // in place; leaves the "other" structure empty.
  void concat(const top_measure_type&, self_type& other) {
    top_layer.concat(other.top_layer);
    check();
    other.check();
  }

  template <class Pred>
  measured_type search_for_chunk(const Pred& p, measured_type prefix,
                                 const Top_item_base*& c) const {
    prefix = top_layer.search(p, prefix, c);
    return prefix;
  }

  template <class Pred>
  measured_type split(const top_measure_type&,
                      const Pred& p,
                      measured_type prefix,
                      value_type& x,
                      self_type& other) {
    cached_item_type v;
    prefix = top_layer.split(p, prefix, v, other.top_layer);
    x = top_item_of_cached_item(v);
    check();
    other.check();
    return prefix;
  }

  // 2-way splitting; assumes `other` to be empty.
  template <class Pred>
  measured_type split(const top_measure_type& meas,
                      const Pred& p,
                      measured_type prefix,
                      self_type& other) {
    value_type v;
    prefix = split(meas, p, prefix, v, other);
    other.push_front(meas, v);
    check();
    other.check();
    return prefix;
  }

  template <class Body>
  void for_each(const Body& f) const {
    top_layer.rec_for_each(0, f);
  }

  measure_type get_measure() const {
    return meas_fct;
  }

  inline void push_front(const value_type& x, measured_type w) {
    cached_item_type v = cached_item_of_top_item(x, w);
    top_layer.push_front(v);
  }

  inline void push_back(const value_type& x, measured_type w) {
    cached_item_type v = cached_item_of_top_item(x, w);
    top_layer.push_back(v);
  }


  // ---for debugging

  inline value_type pop_front() {
    top_measure_type meas;
    return pop_front(meas);
  }

  inline value_type pop_back() {
    top_measure_type meas;
    return pop_back(meas);
  }

  void concat(self_type& other) {
    top_measure_type meas;
    concat(meas, other);
  }

  /* deprecated
  // 3-way splitting; assumes "other" to be initially empty.
  void split(measured_type wtarget, value_type& x, self_type& other) {
    measured_type prefix = algebra_type::identity();
    size_type orig_sz = get_cached();
    size_type nb_target = wtarget + 1;
    auto p = [nb_target] (measured_type m) {
      return nb_target <= m;
    };
    top_measure_type top_meas;
    prefix = split(top_meas, p, prefix, x, other);
    size_type sz = get_cached();
    assert(sz == wtarget);
    size_type other_sz = other.get_cached();
    assert(other_sz + sz == orig_sz);
  }
  */

  // only works when size() = get_cached()
  // 2-way splitting; assumes `other` to be empty.
  void split(size_type n, self_type& other) {
    check();
    other.check();
    size_type size_orig = get_cached();
    assert(n >= 0);
    assert(n <= size_orig);
    if (n == 0) {
      swap(other);
      return; // todo: perform checks
    }
    if (n == size_orig) {
      return; // todo: perform checks
    }
    auto p = [n] (measured_type m) {
      return n+1 <= m;
    };
    top_measured_type prefix = algebra_type::identity();
    top_measure_type top_meas;
    prefix = split(top_meas, p, prefix, other);

    check();
    other.check();
    size_type size_cur = get_cached();
    size_type size_other = other.get_cached();
    //printf(" � %d %d\n", size_cur, size_other);
    assert(size_other + size_cur == size_orig);
    assert(size_cur == n);
    assert(size_other + n == size_orig);
  }

  void check() {
    #ifdef BOOTCHUNKEDSEQ_CHECK
    top_layer.rec_check(depth0);
    //printf("check invariants successful\n");
    #endif
  }

  // TODO: currently ItemPrinter is not used; remove its use?
  template <class ItemPrinter>
  void print() {
    top_layer.rec_print(depth0);
  }

  template <class Add_edge, class Process_chunk>
  void reveal_internal_structure(const Add_edge& add_edge,
                                 const Process_chunk& process_chunk) const {
    add_edge((void*)this, (void*)&top_layer);
    top_layer.rec_reveal_internal_structure(add_edge, process_chunk, depth0);
  }


};






/***********************************************************************/

} // end namespace
} // end namespace
} // end namespace
} // end namespace

#endif /*! _PASL_DATA_BOOTCHUNKEDSEQNEW_H_ */
