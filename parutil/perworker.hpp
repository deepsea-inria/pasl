/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file perworker.hpp
 * \brief Per-worker local storage
 *
 */

#ifndef _PASL_DATA_PERWORKER_H_
#define _PASL_DATA_PERWORKER_H_

#include <assert.h>
#include <initializer_list>

#include "callback.hpp"
#include "worker.hpp"

/***********************************************************************/

namespace pasl {
namespace data {
namespace perworker {
  
/*---------------------------------------------------------------------*/

static constexpr int default_padding_szb = 64 * 2;
static constexpr int default_max_nb_workers = 128;
    
/*---------------------------------------------------------------------*/
/*! \class array
 *  \brief Array indexed by worker id
 *  \tparam Item type of the items to be stored in the container
 *  \tparam padding_szb number of bytes of empty space to add to the 
 *  end of each item in the container
 *  \tparam max_nb_workers one plus the highest worker id to be 
 *  addressed by the container
 *  \ingroup data
 *  \ingroup perworker
 *
 * This container is a polymorphic fixed-capacity container that
 * provides one storage cell for each of PASL worker thread. The
 * purpose is to provide a container that enables workers to make
 * many writes to their own private position of the array and for 
 * one worker to make a few reads from the cells of all the workers.
 * This kind of container is useful for statistics: for example, it
 * can be used to obtain a count of a type of event that occurs
 * frequently during the parallel run of a program.
 *
 * Assuming that the given padding is at least twice the size of
 * a cache line, each cell in the container is guaranteed not
 * to share a cache line with any other cell (or any other piece
 * of memory in the program for that matter). This property is
 * crucial to avoid false sharing on frequent writes to the
 * cells of the container.
 *
 * Access to this container is not synchronized. Thread safety must
 * be enforced by the clients of the container.
 *
 */
template <class Item,
          int padding_szb = default_padding_szb,
          int max_nb_workers = default_max_nb_workers>
class array {
public:
  
  using value_type = Item;
  using index_type = worker_id_t;
  
private:
  
  typedef struct {
    value_type item;
    int padding[padding_szb/4];
  } contents_type;
  
  int padding[padding_szb/4];
  __attribute__ ((aligned (64))) contents_type contents[max_nb_workers];
  
  void check_index(index_type id) const {
    assert(id >= 0);
    assert(id <= max_nb_workers);
    util::worker::the_group.check_worker_id(worker_id_t(id));
  }
  
public:
  
  array() { }
  
  array(std::initializer_list<value_type> l) {
    if (l.size() != 1)
      util::atomic::fatal([] { std::cout << "perworker given bogus initializer list"; });
    init(*(l.begin()));
  }
  
  //! \brief Returns reference to the contents of the cell at position `id` in the array
  value_type& operator[](const index_type id) {
    check_index(id);
    return contents[id].item;
  }
  
  //! \brief Returns reference to the contents of the cell at position `id` in the array
  value_type& operator[](const int id) {
    return operator[](index_type(id));
  }
  
  //! \brief Returns a reference to the cell of the calling worker thread
  value_type& mine() {
    return mine(util::worker::get_my_id());
  }
  
  /*! \brief Returns a reference to the cell of the calling worker thread
   *
   * \pre `my_id` equals the id of the calling worker thread
   */
  value_type& mine(worker_id_t my_id) {
    assert(my_id == util::worker::get_my_id());
    return operator[](my_id);
  }

  //! \brief Applies `body[operator[](i)]` for each `i` in [0, ... nb_workers-1]`
  template <class Body>
  void for_each(const Body& body) {
    auto b = [&] (index_type id) {
      body(id, (*this)[id]);
    };
    util::worker::the_group.for_each_worker(b);
  }
  
  //! \brief Same as `for_each`, except with const access to cells of the array
  template <class Body>
  void cfor_each(const Body& body) const {
    auto b = [&] (index_type id) {
      body(id, contents[id].item);
    };
    util::worker::the_group.for_each_worker(b);
  }
  
  /*! \brief Returns the result of combining the contents of the cells
   * of the array by a given combining operator.
   *
   * Example
   * -------
   * Let `identity = 0`, `comb = [] (int x, int y) { return x + y; }`
   * and `a` be a value of type `array<int>`.
   * Then `x.combine(identity, comb)` equals `a[nb_workers-1] + a[nb_workers-2]
   * + ... + a[0] + identity`.
   *
   * The value is computed sequentially by the calling thread.
   *
   */
  template <class Combiner>
  value_type combine(value_type identity, const Combiner& comb) const {
    value_type res = identity;
    cfor_each([&] (index_type id, value_type v) {
      res = comb(res, v);
    });
    return res;
  }
  
  //! \brief Writes given value into each cell of the array
  void init(const value_type& v) {
    for_each([&] (index_type, value_type& dst) {
      dst = v;
    });
  }
  
};
  
/*---------------------------------------------------------------------*/
/*! \class with_undefined
 *  \brief Per-worker array that can index the undefined worker id
 *  \tparam Base type that must be an instantiation of 
 *  pasl::data:perworker::array.
 *  \ingroup data
 *  \ingroup perworker
 *
 * This container has the same behavior as the perworker array,
 * with the one exception that the container can be indexed by the
 * undefined worker id, namely `worker::undef`.
 *
 */
template <class Array>
class with_undefined {
public:
  
  using value_type = typename Array::value_type;
  using index_type = worker_id_t;
  
private:
  
  value_type ext;
  Array array;
  
public:
  
  with_undefined() { }
  
  with_undefined(std::initializer_list<value_type> l) {
    if (l.size() != 1)
      util::atomic::fatal([] { std::cout << "perworker given bogus initializer list"; });
    init(*(l.begin()));
  }
  
  value_type& operator[](const worker_id_t id_or_undef) {
    return (id_or_undef == util::worker::undef) ? ext : array[id_or_undef];
  }
  
  value_type& operator[](const int id) {
    return operator[](index_type(id));
  }
  
  value_type& mine() {
    return operator[](int(util::worker::get_my_id()));
  }
  
  template <class Body>
  void for_each(const Body& body) {
    body(util::worker::undef, ext);
    array.for_each(body);
  }
  
  template <class Combiner>
  value_type combine(value_type identity, const Combiner& comb) {
    value_type res = comb(identity, ext);
    return array.combine(res, comb);
  }
  
  void init(const value_type& v) {
    ext = v;
    array.init(v);
  }
  
};
  
template <class Item,
          int padding_szb = default_padding_szb,
          int max_nb_workers = default_max_nb_workers>
using extra = with_undefined<array<Item, padding_szb, max_nb_workers>>;
  
#ifdef HAVE_STD_TLS
 
/*---------------------------------------------------------------------*/
/*! \class cell
 *  \brief Worker-local cell
 *  \tparam Item type of the item to be stored in the cell
 *  \ingroup data
 *  \ingroup perworker
 *
 * A cell that is accessible only by the calling thread.
 *
 * The usage of this class is similar to that of the class `array`
 * above, except that the `cell` class exports just one method, namlye 
 * `mine()`.
 *
 */
template <class Item>
class cell {
private:
  
  thread_local Item v;
  Item dflt;
  bool initialized = false;
  
  class callback : public util::callback::client {
  public:
    
    cell<Item>* p;
    
    callback(cell<Item>* p)
    : p(p) { }
    
    void init() {
      p->init();
    }
    
    void destroy() {
      
    }
    
    void output() {
      
    }
  };
  
  void init() {
    initialized = true;
    v = dflt;
  }
  
  void register_callback() {
    util::callback::register_client(new callback(this));
  }
  
public:
  
  using value_type = Item;
  
  cell() {
    register_callback();
  }
  
  cell(value_type v)
  : dflt(v), v(v) {
    register_callback();
  }
  
  value_type& mine() {
    assert(initialized);
    return v;
  }
  
};
  
#else
  
template <class Item>
class cell {
private:
  
  array<Item> cells;
  Item dflt;
  bool initialized = false;
  
  class callback : public util::callback::client {
  public:
    
    cell<Item>* p;
    
    callback(cell<Item>* p)
    : p(p) { }
    
    void init() {
      p->init();
    }
    
    void destroy() {
      
    }
    
    void output() {
      
    }
  };
  
  void init() {
    initialized = true;
    cells.init(dflt);
  }
  
  void register_callback() {
    util::callback::register_client(new callback(this));
  }
  
public:
  
  using value_type = Item;
  
  cell() {
    register_callback();
  }
  
  cell(value_type v) : dflt(v) {
    register_callback();
  }
  
  value_type& mine() {
    assert(initialized);
    return cells.mine();
  }
  
};
  
#endif
  
/*---------------------------------------------------------------------*/
/* Per-worker counters */
  
namespace counter {
  
/*! \class counter
 *  \brief Distributed counter
 *  \tparam Number type to use for the values of the counter cells
 *  \tparam Base type to select the perworker storage
 *  \ingroup data
 *  \ingroup perworker
 *
 * A distributed counter is a perworker array
 *
 * Cells are not initialized automatically.
 *
 */
  
template <
  class Number,
  template <
    class Item,
    int padding_szb = default_padding_szb,
    int max_nb_workers = default_max_nb_workers
  >
  class Array = array
>
class carray {
private:
  
  using perworker_type = Array<Number>;
  
  perworker_type counters;
  
public:
  
  using number_type = Number;
  using index_type = typename perworker_type::index_type;
  
  carray() { }
  
  carray(std::initializer_list<number_type> l) {
    if (l.size() != 1)
      util::atomic::fatal([] { std::cout << "perworker given bogus initializer list"; });
    init(*(l.begin()));
  }
  
  //! \brief Copies `v` to each cell of the array
  void init(number_type v) {
    counters.init(v);
  }
  
  //! \brief Returns reference to the contents of the cell at position `id` in the array
  number_type& operator[](const index_type id) {
    return counters[id];
  }
  
  //! \brief Returns reference to the contents of the cell at position `id` in the array
  number_type& operator[](const int id) {
    return counters[id];
  }
  
  /*! \brief Adjusts the value in the cell of the calling worker thread by `d`
   *
   * \pre `my_id` equals the id of the calling worker thread
   */
  number_type delta(worker_id_t my_id, number_type d) {
    counters.mine(my_id) += d;
    return counters.mine(my_id);
  }
  
  /*! \brief Increments the value in the cell of the calling worker thread by `+d`
   *
   * \pre `my_id` equals the id of the calling worker thread
   */
  number_type incr(worker_id_t my_id, number_type d) {
    return delta(my_id, d);
  }
  
  /*! \brief Decrements the value in the cell of the calling worker thread by `-d`
   *
   * \pre `my_id` equals the id of the calling worker thread
   */
  number_type decr(worker_id_t my_id, number_type d) {
    return delta(my_id, -d);
  }
  
  //! \brief Increments the value in the cell of the calling worker thread by one
  number_type operator++(int) {
    return incr(util::worker::get_my_id(), 1l);
  }
  
  //! \brief Decrements the value in the cell of the calling worker thread by one
  number_type operator--(int) {
    return decr(util::worker::get_my_id(), 1l);
  }
  
  //! \brief Returns the sum of all the values in the array
  number_type sum() const {
    return counters.combine(0l, [] (number_type x, number_type y) { return x + y; });
  }
  
  void store(number_type v) {
    counters.init(v);
  }
  
  number_type load() const {
    return sum();
  }
  
};
  
template <class Number>
using extra = carray<Number, extra>;
  
} // end namespace

  
} // end namespace
} // end namespace
} // end namespace

/***********************************************************************/

#endif /*! _PASL_DATA_PERWORKER_H_ */
