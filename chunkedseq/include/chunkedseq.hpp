/*!
 * \author Umut A. Acar
 * \author Arthur Chargueraud
 * \author Mike Rainey
 * \date 2013-2018
 * \copyright 2014 Umut A. Acar, Arthur Chargueraud, Mike Rainey
 *
 * \brief Chunked-sequence functor
 * \file chunkedseq.hpp
 *
 */

#include <cstddef>

#include "fixedcapacity.hpp"
#include "chunk.hpp"
#include "cachedmeasure.hpp"
#include "chunkedseqbase.hpp"
#include "bootchunkedseq.hpp"
#include "ftree.hpp"

#ifndef _PASL_DATA_CHUNKEDSEQ_H_
#define _PASL_DATA_CHUNKEDSEQ_H_

namespace pasl {
namespace data {
namespace chunkedseq {

/***********************************************************************/

/*---------------------------------------------------------------------*/
/* Place to put configuration classes for defining various
 * instantiations of the chunked-sequence functor
 */

template <
  class Item,
  int Chunk_capacity = 512,
  class Client_cache = cachedmeasure::trivial<Item, size_t>,
  template <
    class Chunk_item,
    int Capacity,
    class Item_alloc
  >
  class Chunk_struct = fixedcapacity::heap_allocated::ringbuffer_ptr,
  template <
    class Top_item_base,
    int ___Chunk_capacity,
    class ___Cached_measure,
    class Top_item_deleter,
    class Top_item_copier,
    template <
      class ___Item,
      int ___Capacity,
      class ___Item_alloc
    >
    class ___Chunk_struct,
    class Size_access
  >
  class Middle_sequence = bootchunkedseq::cdeque,
  class Item_alloc = std::allocator<Item>
>
class basic_deque_configuration {
public:

  using size_type = typename Client_cache::size_type;
  using difference_type = ptrdiff_t;  // later: check if this is ok

  using value_type = Item;
  using segment_type = segment<value_type*>;

  static constexpr size_type chunk_capacity = size_type(Chunk_capacity);

  using item_allocator_type = Item_alloc;

  using item_queue_type = Chunk_struct<value_type, Chunk_capacity, item_allocator_type>;

  using client_algebra_type = typename Client_cache::algebra_type;
  using size_algebra_type = algebra::int_group_under_addition_and_negation<size_type>;
  using middle_algebra_type = algebra::combiner<size_algebra_type, client_algebra_type>;
  using middle_measured_type = typename middle_algebra_type::value_type;

  using chunk_cache_type = Client_cache;
  using chunk_measured_type = typename chunk_cache_type::measured_type;
#ifdef DISABLE_RANDOM_ACCESS_ITERATOR
  using cached_prefix_type = annotation::without_measured;
#else
  using cached_prefix_type = annotation::with_measured<middle_measured_type>;
#endif
  //! \todo fix bug in finger search that is causing iterator-based traversal to fail unit test in mode -generate_by_insert 1
  //! \todo once finger search bug is fixed, enable finger search by default
#ifndef ENABLE_FINGER_SEARCH
  using parent_pointer_type = annotation::without_parent_pointer;
#else
  using parent_pointer_type = annotation::with_parent_pointer<middle_measured_type>;
#endif
  using annotation_type = annotation::annotation_builder<cached_prefix_type, parent_pointer_type>;
  using chunk_type = chunk<item_queue_type, chunk_cache_type, annotation_type>;

  class middle_cache_type {
  public:

    using size_type = typename Client_cache::size_type;
    using value_type = const chunk_type*;
    using algebra_type = middle_algebra_type;
    using measured_type = typename algebra_type::value_type;

    class measure_type {
    public:

      using client_measure_type = typename Client_cache::measure_type;
      using client_value_type = typename chunk_type::value_type;

    private:

      client_measure_type client_meas;

    public:

      measure_type() { }

      measure_type(const client_measure_type& client_meas)
      : client_meas(client_meas) { }

      measured_type operator()(const client_value_type& v) const {
        measured_type m = algebra_type::identity();
        m.value1 = size_type(1);
        m.value2 = client_meas(v);
        return m;
      }

      measured_type operator()(const client_value_type* lo, const client_value_type* hi) const {
        measured_type m = algebra_type::identity();
        for (auto p = lo; p < hi; p++)
          m = algebra_type::combine(m, operator()(*p));
        return m;
      }

      measured_type operator()(value_type p) const {
        measured_type m = algebra_type::identity();
        m.value1 = p->size();
        m.value2 = p->get_cached();
        return m;
      }

      measured_type operator()(const value_type* lo, const value_type* hi) const {
        measured_type m = algebra_type::identity();
        for (auto p = lo; p < hi; p++)
          m = algebra_type::combine(m, operator()(*p));
        return m;
      }

      client_measure_type get_client_measure() const {
        return client_meas;
      }

      void set_client_measure(client_measure_type client_meas) {
        this->client_meas = client_meas;
      }

    };

    static void swap(measured_type& x, measured_type& y) {
      std::swap(x.value1, y.value1);
      std::swap(x.value2, y.value2);
    }

  };

  using middle_measure_type = typename middle_cache_type::measure_type;

  class size_access {
  public:
    using client_measured_type = chunk_measured_type;

    static constexpr bool enable_index_optimization = true;

    static size_type& size(middle_measured_type& m) {
      return m.value1;
    }
    static size_type csize(middle_measured_type m) {
      return m.value1;
    }
    static client_measured_type& client(middle_measured_type& m) {
      return m.value2;
    }
    static client_measured_type cclient(middle_measured_type m) {
      return m.value2;
    }
  };

#ifdef DEBUG_MIDDLE_SEQUENCE
  //  use chunk as middle sequence; for the purpose of testing the chunkedseq functor in isolation.
  static constexpr int middle_capacity = 1<<23;
  using chunk_pointer_queue_type = fixedcapacity::heap_allocated::ringbuffer_ptr<chunk_type*, middle_capacity>;
  using middle_annotation_type = annotation::annotation_builder<>;
  using middle_type = chunk<chunk_pointer_queue_type, middle_cache_type,
                            middle_annotation_type, Pointer_deleter, Pointer_deep_copier, size_access>;
#else
  static constexpr int middle_chunk_capacity = 32; // 32 64 128;
  using middle_type = Middle_sequence<chunk_type, middle_chunk_capacity, middle_cache_type,
  Pointer_deleter, Pointer_deep_copier, fixedcapacity::heap_allocated::ringbuffer_ptr, size_access>;
#endif

  using chunk_search_type = itemsearch::search_in_chunk<chunk_type, middle_algebra_type, size_access>;

};

/*---------------------------------------------------------------------*/
/* Instantiations for the bootstrapped chunked sequence */

namespace bootstrapped {

// Application of chunked deque to a configuration

template <
  class Item,
  int Chunk_capacity=512,
  class Cache = cachedmeasure::trivial<Item, size_t>,
  template <
    class Chunk_item,
    int Capacity,
    class Chunk_item_alloc = std::allocator<Item>
  >
  class Chunk_struct = fixedcapacity::heap_allocated::ringbuffer_ptrx,
  class Item_alloc = std::allocator<Item>
>
using deque = chunkedseqbase<basic_deque_configuration<Item, Chunk_capacity, Cache, Chunk_struct, bootchunkedseq::cdeque, Item_alloc>>;

// Application of chunked stack to a configuration

template <
  class Item,
  int Chunk_capacity = 512,
  class Cache = cachedmeasure::trivial<Item, size_t>,
  class Item_alloc = std::allocator<Item>
>
using stack = deque<Item, Chunk_capacity, Cache, fixedcapacity::heap_allocated::stack, Item_alloc>;

} // end namespace bootstrapped

/*---------------------------------------------------------------------*/
/* Instantiations for the finger tree */

namespace ftree {

// Application of a chunked finger tree to a configuration

template <
  class Item,
  int Chunk_capacity = 512,
  class Cache = cachedmeasure::trivial<Item, size_t>,
  template <
    class Chunk_item,
    int Capacity,
    class Chunk_item_alloc = std::allocator<Item>
  >
  class Chunk_struct = fixedcapacity::heap_allocated::ringbuffer_ptrx,
  class Item_alloc = std::allocator<Item>
>
using deque = chunkedseqbase<basic_deque_configuration<Item, Chunk_capacity, Cache, Chunk_struct, ::pasl::data::ftree::tftree, Item_alloc>>;


// Application of chunked finger tree to a configuration

template <
  class Item,
  int Chunk_capacity = 512,
  class Cache = cachedmeasure::trivial<Item, size_t>,
  class Item_alloc = std::allocator<Item>
>
using stack = deque<Item, Chunk_capacity, Cache, fixedcapacity::heap_allocated::stack, Item_alloc>;

} // end namespace ftree

/***********************************************************************/

} // end namespace
} // end namespace
} // end namespace

#endif /*! _PASL_DATA_CHUNKEDSEQ_H_ */
