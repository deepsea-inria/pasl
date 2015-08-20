/* COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file pmem.hpp
 * \brief Basic allocation and memory transfer operations
 *
 */

#include "ploop.hpp"

#include "callable.hpp"

#include <type_traits>
#include <iterator>
#include <algorithm>
#include <memory>
#include <utility>

#ifndef _PCTL_PMEM_H_
#define _PCTL_PMEM_H_

namespace pasl {
namespace pctl {
namespace pmem {

/***********************************************************************/

/*---------------------------------------------------------------------*/
/* Primitive memory operations */

namespace _detail {
/*! conversion between integral types and iterators relatively to an offset
 */
template<class IndexT, class Iterator>
auto index_of(Iterator, IndexT idx) ->
typename std::enable_if<!std::is_integral<IndexT>::value, IndexT>::type {
  return idx;
}

template<class IndexT, class Iterator>
auto index_of(Iterator offset, Iterator idx) ->
typename std::enable_if<
           std::is_integral<IndexT>::value &&
           !std::is_integral<Iterator>::value,
           IndexT
         >::type {
  return idx - offset;
}

template<class IndexT, class Iterator, class Int>
auto index_of(Iterator, Int idx) ->
typename std::enable_if<
           std::is_integral<IndexT>::value && std::is_integral<Int>::value,
           IndexT
         >::type {
  return idx;
}

template<class IndexT, class Iterator, class Int>
auto index_of(Iterator offset, Int idx) ->
typename std::enable_if<
           !std::is_integral<IndexT>::value && std::is_integral<Int>::value,
           IndexT
         >::type {
  return offset + idx;
}
  
/**
 * \brief Checks whether Provider is a valid provider object.
 *
 * \tparam Provider the type to be checked
 * \tparam Value    a type to check Provider against
 * \tparam Iterator a type to check Provider against
 *
 * A valid provider must:
 * - be a function object (i.e. std::declval<Provider>()(...) must be a valid
 *   call)
 * - return an object whose type is convertible to Value
 * - take exactly one argument which is either an Iterator or an integral
 *
 * If Provider is a valid provider object, this struct inherits from
 * <tt>std::true_type</tt>, otherwise, it inherits from
 * <tt>std::false_type</tt>.
 */
template<class Provider, class Value, class Iterator, class = void>
struct is_valid_provider : std::false_type {};

/**
 * \cond 0
 */
template<class Provider, class Value, class Iterator>
struct is_valid_provider<
         Provider, Value, Iterator,
         typename std::enable_if<
                    ::util::callable_traits<Provider>::function_object &&
                    std::is_convertible<
                      typename ::util::callable_traits<Provider>::return_type,
                      Value
                    >::value &&
                    ::util::callable_traits<Provider>::argument_count == 1 &&
                    (std::is_same<
                       ::util::argument_t<Provider, 0>, Iterator
                     >::value ||
                     std::is_integral<::util::argument_t<Provider, 0>>::value)
                  >::type
       > : std::true_type {};
/**
 * \endcond
 */
  
/**
 * \brief Checks whether WorkEstimator is a valid work estimator object.
 *
 * \tparam WorkEstimator the type to be checked
 * \tparam Iterator      a type to check WorkEstimator against
 *
 * A valid work estimator must:
 * - be a function object (i.e. std::declval<WorkEstimator>()(...) must be a
 *   valid call)
 * - return an integral type
 * - take exactly two arguments of type Iterator
 *
 * If WorkEstimator is a valid work estimator object, this struct inherits from
 * <tt>std::true_type</tt>, otherwise, it inherits from
 * <tt>std::false_type</tt>.
 */
template<class WorkEstimator, class Iterator, class = void>
struct is_valid_estimator : std::false_type {};

/**
 * \cond 0
 */
template<class WorkEstimator, class Iterator>
struct is_valid_estimator<
         WorkEstimator, Iterator,
         typename std::enable_if<
                    ::util::callable_traits<WorkEstimator>::function_object &&
                    std::is_integral<
                      typename ::util::callable_traits<WorkEstimator>::
                               return_type
                    >::value &&
                    ::util::callable_traits<WorkEstimator>::argument_count == 2
// the following cause errors with g++ 4.8.3 (not fully c++11 compliant)
// this relax the WorkEstimator constraints and may cause ambiguous calls
#if !defined(__GNUG__) || defined(__clang__) || defined(__INTEL_COMPILER) ||  \
    (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__) >= 40900
                    &&
                    std::is_convertible<
                      Iterator, ::util::argument_t<WorkEstimator, 0>
                    >::value &&
                    std::is_convertible<
                      Iterator, ::util::argument_t<WorkEstimator, 1>
                    >::value
#endif
                  >::type
       > : std::true_type {};
/**
 * \endcond
 */

// checks whether we use an old version of libstdc++
// (for g++ 4.8.3 compatibility)
#if !defined(__GLIBCXX__) || __GLIBCXX__ > 20140911
template<class T>
using is_trivially_copy_constructible = std::is_trivially_copy_constructible<T>;
  
template<class T>
using is_trivially_copyable = std::is_trivially_copyable<T>;
#else
template<class T>
using is_trivially_copy_constructible = std::is_trivial<T>;

template<class T>
using is_trivially_copyable = std::is_trivial<T>;
#endif
} // namespace _detail

/**
 * \copydoc _detail::is_valid_provider
 */
template<class Provider, class Value, class Iterator>
struct is_valid_provider :
_detail::is_valid_provider<Provider, Value, Iterator> {};

/**
 * \copydoc _detail::is_valid_estimator
 */
template<class WorkEstimator, class Iterator>
struct is_valid_estimator :
_detail::is_valid_estimator<WorkEstimator, Iterator> {};

/* \brief Initializes a range of uninitialized allocated memory by either
 * assigning or calling the copy constructor of its objects with a given value.
 *
 * \pre
 * - [low, high) is allocated
 * - [low, high) is uninitialized (i.e. it contains either trivially
 *   destructible objects or trivially constructible objects or these objects
 *   haven't been constructed)
 *
 * \post
 * - [low, high) is initialized (i.e. it contains valid instances)
 * - each object [low, high) has been assigned provider(i) where i is either
 *   the integral index of this element in the range (starting at 0) or
 *   an iterator pointing on this element depending on the type of provider
 *
 * \tparam Iter          an iterator type satisfying the RandomAccessIterator
 *                       and OutputIterator concepts, in particular,
 *                       <tt>Iter::operator*()</tt> must return an lvalue
 *                       reference.
 * \tparam WorkEstimator a reference to a type satisfying the FunctionObject
 *                       concept returning an integral type and taking two
 *                       Iter as arguments
 * \tparam Provider      a reference to a type satisfying the FunctionObject
 *                       concept returning a type convertible to <tt>typename
 *                       std::iterator_traits<Iter>::value_type</tt>
 *                       and taking either an Iter or an integral type as
 *                       argument
 * \tparam Allocator     a reference to an allocator type satisfying the
 *                       Allocator concept
 *
 * \param low                 the beginning of the range to initialize
 *                            (included)
 * \param high                the end of the range to initialize (excluded)
 * \param range_work_function a function object such that
 *                            <tt>range_work_function(low', high')</tt> returns
 *                            an estimation of the expected work performed by
 *                            the construction of the objects in [low', high')
 * \param provider            a function object such that <tt>provider(idx)</tt>
 *                            returns an Item to be assigned to the idx-th
 *                            object of [low, high) if provider takes an
 *                            integral argument or the object pointed by idx
 *                            if provider takes an iterator as argument
 * \param allocator           a custom allocator
 */
template<
  class Iter, class WorkEstimator, class Provider,
  class Allocator = std::allocator<
                      typename std::iterator_traits<Iter>::value_type
                    >
>
auto tabulate(Iter low, Iter high, WorkEstimator&& range_work_function,
              Provider&& provider,
              Allocator&& allocator = Allocator()) ->
typename std::enable_if<
           is_valid_estimator<WorkEstimator, Iter>::value &&
           is_valid_provider<
             Provider, typename std::iterator_traits<Iter>::value_type, Iter
           >::value
         >::type {
  // aliases
  using provider_traits = ::util::callable_traits<Provider>;
  using allocator_traits =
  std::allocator_traits<typename std::remove_reference<Allocator>::type>;
  using index_t = typename provider_traits::template argument_type<0>;
  using value_type = typename std::iterator_traits<Iter>::value_type;
  using namespace _detail;
  // implementation
  Iter offset = low;
  if(is_trivially_copy_constructible<value_type>::value &&
     is_trivially_copyable<value_type>::value) {
    range::parallel_for(low, high, range_work_function,
    [&, offset] (Iter emplace_iter) {
      *emplace_iter = provider(index_of<index_t, Iter>(offset, emplace_iter));
    }, [&, offset] (Iter low, Iter high) {
      for( Iter emplace_iter = low ; emplace_iter != high ; ++emplace_iter )
        *emplace_iter = provider(index_of<index_t, Iter>(offset, emplace_iter));
    });
  } else {
    range::parallel_for(low, high, range_work_function,
    [&, offset] (Iter emplace_iter) {
      allocator_traits::
      construct(allocator, &(*emplace_iter),
                provider(index_of<index_t, Iter>(offset, emplace_iter)));
    }, [&, offset] (Iter lo, Iter hi) {
      for( Iter emplace_iter = lo; emplace_iter != hi; ++emplace_iter )
        allocator_traits::
        construct(allocator, &(*emplace_iter),
                  provider(index_of<index_t, Iter>(offset, emplace_iter)));
    });
  }
}

/* \overload
 * void tabulate(Iter, Iter, WorkEstimator&&, Provider&&, Allocator&&)
 */
template<
  class Iter, class Provider,
  class Allocator =
  std::allocator<typename std::iterator_traits<Iter>::value_type >
>
auto tabulate(Iter low, Iter high, Provider&& provider,
              Allocator&& allocator = Allocator()) ->
typename std::enable_if<
           is_valid_provider<
             Provider, typename std::iterator_traits<Iter>::value_type, Iter
           >::value
         >::type {
  tabulate(low, high, [] (Iter lo, Iter hi) { return hi - lo; },
           std::forward<Provider>(provider),
           std::forward<Allocator>(allocator));
}

/** todo
 */
template<
  class Iter, class Item,
  class Allocator =
  std::allocator<typename std::iterator_traits<Iter>::value_type>
>
void fill(Iter low, Iter high, const Item& value,
          Allocator&& allocator = Allocator()) {
  // aliases
  using allocator_traits =
  std::allocator_traits<typename std::remove_reference<Allocator>::type>;
  using value_type = typename std::iterator_traits<Iter>::value_type;
  static_assert(std::is_convertible<Item, value_type>::value,
                "fill(Iter, Iter, const Item&, Allocator&&) requires Item to "
                "be convertible to typename "
                "std::iterator_traits<Iter>::value_type.");
  using namespace _detail;
  // implementation
  if(is_trivially_copy_constructible<value_type>::value &&
     is_trivially_copyable<value_type>::value) {
    range::parallel_for(low, high, [] (Iter low, Iter high) {
      return high - low;
    }, [&value] (Iter emplace_iter) {
      std::fill(emplace_iter, emplace_iter + 1, value);
    }, [&value] (Iter low, Iter high) {
      std::fill(low, high, value);
    });
  } else {
    range::parallel_for(low, high, [] (Iter low, Iter high) {
      return high - low;
    }, [&value, &allocator] (Iter emplace_iter) {
      allocator_traits::construct(allocator, &(*emplace_iter), value);
    }, [&value, &allocator] (Iter lo, Iter hi) {
      for( Iter emplace_iter = lo; emplace_iter != hi; ++emplace_iter )
        allocator_traits::construct(allocator, &(*emplace_iter), value);
    });
  }
}

/* \brief Copies a range.
 *
 * \pre
 * - [low, high) is allocated
 * - [low, high) is initialized (i.e. it contains valid instances)
 *
 * \tparam Iter            an iterator type satisfying the RandomAccessIterator
 *                         concept
 * \tparam Output_iterator an iterator satisfying the RandomAccessIterator and
 *                         OutputIterator concepts
 */
template <class Iter, class Output_iterator>
void copy(Iter lo, Iter hi, Output_iterator dst) {
  range::parallel_for(lo, hi, [&] (Iter lo, Iter hi) { return hi - lo; }, [&] (Iter i) {
    std::copy(i, i + 1, dst + (i - lo));
  }, [&] (Iter lo2, Iter hi2) {
    std::copy(lo2, hi2, dst + (lo2 - lo));
  });
}

/* \brief Destroys objects in a range in parallel.
 *
 * \pre
 * - [low, high) is allocated
 * - [low, high) is initialized (i.e. it contains valid instances)
 *
 * \tparam Iter      an iterator type satisfying the RandomAccessIterator
 *                   concept, in particular, Iter::operator* must return an
 *                   lvalue reference
 * \tparam Allocator an allocator satisfying the Allocator concept
 *
 * \param low       the beginning of the range to destroy (included)
 * \param high      the end of the range to destroy (excluded)
 * \param allocator a custom allocator
 */
template<class Iter, class Allocator>
auto pdelete(Iter low, Iter high, Allocator&& allocator = Allocator()) ->
typename std::enable_if<
           !std::is_same<
              typename std::iterator_traits<Iter>::value_type,
              typename std::allocator_traits<
                         typename std::remove_reference<Allocator>::type
                       >::value_type
            >::value
         >::type {
  using allocator_type =
  typename std::allocator_traits<
             typename std::remove_reference<Allocator>::type
           >::template rebind_alloc<
                         typename std::iterator_traits<Iter>::value_type
                       >::other;
  allocator_type rebound(allocator);
  pdelete(low, high, rebound);
}

/* \overload
 * void pdelete(Iter low, Iter high, Allocator& allocator)
 */
template<
  class Iter,
  class Allocator =
  std::allocator<typename std::iterator_traits<Iter>::value_type>
>
auto pdelete(Iter low, Iter high, Allocator&& allocator = Allocator()) ->
typename std::enable_if<
           std::is_same<
             typename std::iterator_traits<Iter>::value_type,
             typename std::allocator_traits<
                        typename std::remove_reference<Allocator>::type
                      >::value_type
           >::value
         >::type {
  using value_type = typename std::iterator_traits<Iter>::value_type;
  if(!std::is_trivially_destructible<value_type>::value)
    parallel_for(low, high, [&allocator] (Iter it) {
      std::allocator_traits<
        typename std::remove_reference<Allocator>::type
      >::destroy(allocator, &(*it));
    });
}

/***********************************************************************/

} // end namespace
} // end namespace
} // end namespace

#endif /*! _PCTL_PMEM_H_ */
