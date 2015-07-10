/* COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file parray.hpp
 * \brief Array-based implementation of sequences
 *
 */

#include <cmath>

#include "datapar.hpp"
#include "chunkedseq.hpp"

#ifndef _PCTL_PSET_BASE_H_
#define _PCTL_PSET_BASE_H_

namespace pasl {
namespace pctl {

/***********************************************************************/

/*---------------------------------------------------------------------*/
/* Parallel set */

namespace {
  
  template <class Item>
  class optional {
  public:
    
    using self_type = optional<Item>;
    
    Item item;
    bool no_item;
    
    optional()
    : item(), no_item(true) { }
    
    optional(Item item)
    : item(item), no_item(false) { }
    
    optional(const optional& other)
    : item(other.item), no_item(other.no_item) { }
    
    void swap(self_type& other) {
      std::swap(item, other.item);
      std::swap(no_item, other.no_item);
    }
    
  };
  
  template <class Item, class Measured>
  class get_last_item {
  public:
    
    using value_type = Item;
    using measured_type = Measured;
    
    measured_type operator()(const value_type& v) const {
      return measured_type(v);
    }
    
    measured_type operator()(const value_type* lo, const value_type* hi) const {
      if (hi - lo == 0) {
        return measured_type();
      } else {
        return measured_type(*(hi - 1));
      }
    }
  };

  template <class Option>
  class take_right_if_nonempty {
  public:
    
    using value_type = Option;
    
    static constexpr bool has_inverse = false;
    
    static value_type identity() {
      return value_type();
    }
    
    static value_type combine(value_type left, value_type right) {
      if (right.no_item) {
        return left;
      } else {
        return right;
      }
    }
    
    static value_type inverse(value_type x) {
      util::atomic::fatal([] {
        std::cout << "inverse operation not supported" << std::endl;
      });
      return identity();
    }
    
  };

  //! [pset_cache]
  template <class Item_ptr, class Size>
  class pset_cache {
  public:
    
    using size_type = Size;
    using value_type = Item_ptr;
    using optional_type = optional<value_type>;
    using algebra_type = take_right_if_nonempty<optional_type>;
    using measured_type = typename algebra_type::value_type; // = optional_type
    using measure_type = get_last_item<value_type, measured_type>;
    
    static void swap(measured_type& x, measured_type& y) {
      x.swap(y);
    }
    
  };
  
  template <class Item>
  class pset_merge_chunkedseq_contr {
  public:
    static controller_type contr;
  };
  
  template <class Item>
  controller_type pset_merge_chunkedseq_contr<Item>::contr("pset_merge"+sota<Item>());
  
  template <class Item>
  class pset_intersect_chunkedseq_contr {
  public:
    static controller_type contr;
  };
  
  template <class Item>
  controller_type pset_intersect_chunkedseq_contr<Item>::contr("pset_intersect"+sota<Item>());
  
  template <class Item>
  class pset_diff_chunkedseq_contr {
  public:
    static controller_type contr;
  };
  
  template <class Item>
  controller_type pset_diff_chunkedseq_contr<Item>::contr("pset_diff"+sota<Item>());
  
} // end namespace
  
  template <class value_type, int chunk_capacity, class cache_type>
  std::ostream& operator<<(std::ostream& out, const data::chunkedseq::bootstrapped::deque<value_type, chunk_capacity, cache_type>& xs) {
    out << "{ ";
    for (auto it = xs.cbegin(); it != xs.cend(); it++) {
      auto x = *it;
      out << x;
      if (it+1 != xs.cend())
        out << ", ";
    }
    out << " }";
    return out;
  }
  
template <
  class Item,
  class Compare = std::less<Item>,
  class Alloc = std::allocator<Item>,
  int chunk_capacity = 8
>
class pset {
public:
  
  using key_type = Item;
  using value_type = Item;
  using key_compare = Compare;
  using allocator_type = Alloc;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  using difference_type = ptrdiff_t;
  using size_type = size_t;
  
private:
  
  using cache_type = pset_cache<value_type, size_type>;
  using container_type = data::chunkedseq::bootstrapped::deque<value_type, chunk_capacity, cache_type>;
  using option_type = typename cache_type::measured_type;
  
public:
  
  using iterator = typename container_type::iterator;
  using const_iterator = typename container_type::const_iterator;
  
private:
  
  // invariant: items in seq are sorted in ascending order by their key values
  mutable container_type seq;
  mutable iterator it;
  
  static bool option_compare(option_type lhs, option_type rhs) {
    if (lhs.no_item && rhs.no_item) {
      return false;
    }
    if (lhs.no_item) {
      return true;
    }
    if (rhs.no_item) {
      return false;
    }
    key_compare comp;
    return comp(lhs.item, rhs.item);
  }
  
  static bool same_key(const key_type& lhs, const key_type& rhs) {
    key_compare comp;
    return (!comp(lhs, rhs)) && (!comp(rhs, lhs));
  }
  
  static bool same_option(const option_type& lhs, const option_type& rhs) {
    return ((!option_compare(lhs, rhs)) && (!option_compare(rhs, lhs)));
  }
  
  static bool less_than_or_equal(const option_type& lhs, const option_type& rhs) {
    return option_compare(lhs, rhs) || same_option(lhs, rhs);
  }
  
  /* returns either
   *   - the position of the first item in the
   *     sequence `seq` that is either larger than
   *     or equal to `k`, if there is such a position
   *   - the position one past the end of `seq`, otherwise
   */
  iterator first_larger_or_eq(const key_type& k) {
    option_type target(k);
    it.search_by([&] (const option_type& key) {
      return less_than_or_equal(target, key);
    });
    return it;
  }
  
  const_iterator first_larger_or_eq(const key_type& k) const {
    option_type target(k);
    const_iterator it = cbegin();
    it.search_by([&] (const option_type& key) {
      return less_than_or_equal(target, key);
    });
    return it;
  }
  
  static container_type merge_seq(container_type& xs, container_type& ys) {
    container_type result;
    key_compare compare;
    long n = xs.size();
    long m = ys.size();
    while (true) {
      if (n < m) {
        std::swap(n, m);
        xs.swap(ys);
      }
      if (n == 0) {
        // xs.empty() implies that ys.empty()
        break;
      } else if (m == 0) {
        if (same_key(xs.front(), result.back())) {
          xs.pop_front();
          n--;
        }
        result.concat(xs);
        break;
      } else {
        value_type x = xs.front();
        value_type y = ys.front();
        if (compare(x, y)) {
          xs.pop_front();
          result.push_back(x);
          n--;
        } else if (compare(y, x)) {
          ys.pop_front();
          result.push_back(y);
          m--;
        } else {
          xs.pop_front();
          n--;
        }
      }
    }
    return result;
  }
  
  static container_type merge(container_type& xs, container_type& ys) {
    using controller_type = pset_merge_chunkedseq_contr<Item>;
    key_compare compare;
    long n = xs.size();
    long m = ys.size();
    container_type result;
    par::cstmt(controller_type::contr, [&] { return n + m; }, [&] {
      if (n < m) {
        result = merge(ys, xs);
      } else if (n == 0) {
        result = { };
      } else if (n == 1) {
        if (m == 0) {
          result.push_back(xs.back());
        } else if (same_key(xs.back(), ys.back())) {
          result.push_back(xs.back());
        } else {
          result.push_back(std::min(xs.back(), ys.back(), compare));
          result.push_back(std::max(xs.back(), ys.back(), compare));
        }
      } else {
        container_type xs2;
        xs.split((size_t)n/2, xs2);
        value_type mid = xs.back();
        container_type ys2;
        ys.split([&] (const option_type& key) {
          return option_compare(option_type(mid), key);
        }, ys2);
        container_type result2;
        par::fork2([&] {
          result = merge(xs, ys);
        }, [&] {
          result2 = merge(xs2, ys2);
        });
        result.concat(result2);
      }
    }, [&] {
      result = merge_seq(xs, ys);
    });
    return result;
  }
  
  static container_type merge_par(container_type& xs, container_type& ys) {
    return merge(xs, ys);
  }
  
  static container_type intersect_seq(container_type& xs, container_type& ys) {
    key_compare compare;
    container_type result;
    long n = xs.size();
    long m = ys.size();
    while (true) {
      if ((n == 0) || (m == 0)) {
        break;
      }
      value_type x = xs.front();
      value_type y = ys.front();
      if (compare(x, y)) {
        xs.pop_front();
        n--;
      } else if (compare(y, x)) {
        ys.pop_front();
        m--;
      } else { // then, x == y
        xs.pop_front();
        n--;
        ys.pop_front();
        m--;
        result.push_back(x);
      }
    }
    xs.clear();
    ys.clear();
    return result;
  }
  
  static container_type intersect(container_type& xs, container_type& ys) {
    using controller_type = pset_intersect_chunkedseq_contr<Item>;
    long n = xs.size();
    long m = ys.size();
    container_type result;
    par::cstmt(controller_type::contr, [&] { return n + m; }, [&] {
      if (n < m) {
        result = intersect(ys, xs);
      } else if (n == 0) {
        result = { };
      } else if ((n == 1) && (m == 1) && same_key(xs.back(), ys.back())) {
        result.push_back(xs.back());
      } else if (n == 1) {
        result = { };
      } else {
        container_type xs2;
        xs.split((size_t)n/2, xs2);
        value_type mid = xs.back();
        container_type ys2;
        ys.split([&] (const option_type& key) {
          return option_compare(option_type(mid), key);
        }, ys2);
        container_type result2;
        par::fork2([&] {
          result = intersect(xs, ys);
        }, [&] {
          result2 = intersect(xs2, ys2);
        });
        result.concat(result2);
      }
    }, [&] {
      result = intersect_seq(xs, ys);
    });
    return result;
  }
  
  static container_type diff_seq(container_type& xs, container_type& ys) {
    key_compare compare;
    container_type result;
    long n = xs.size();
    long m = ys.size();
    while (true) {
      if ((n == 0) || (m == 0)) {
        break;
      }
      value_type x = xs.front();
      value_type y = ys.front();
      if (compare(x, y)) {
        result.push_back(x);
        xs.pop_front();
        n--;
      } else if (compare(y, x)) {
        ys.pop_front();
        m--;
      } else {
        xs.pop_front();
        n--;
        ys.pop_front();
        m--;
      }
    }
    ys.clear();
    result.concat(xs);
    return result;
  }
  
  // result := xs - ys
  static container_type diff(container_type& xs, container_type& ys) {
    using controller_type = pset_diff_chunkedseq_contr<Item>;
    long n = xs.size();
    long m = ys.size();
    container_type result;
    par::cstmt(controller_type::contr, [&] { return n + m; }, [&] {
      if (m == 0) {
        result = std::move(xs);
      } else if (n == 0) {
        result = { };
      } else if (n == 1) {
        iterator it = ys.begin();
        it.search_by([&] (const option_type& key) {
          return less_than_or_equal(option_type(xs.back()), key);
        });
        if (it == ys.end()) {
          result = std::move(xs);
        } else if (! same_option(*it, option_type(xs.back()))) {
          result = std::move(xs);
        } else {
          result = { };
        }
      } else {
        container_type xs2;
        xs.split((size_t)n/2, xs2);
        value_type mid = xs.back();
        container_type ys2;
        ys.split([&] (const option_type& key) {
          return option_compare(option_type(mid), key);
        }, ys2);
        container_type result2;
        par::fork2([&] {
          result = diff(xs, ys);
        }, [&] {
          result2 = diff(xs2, ys2);
        });
        result.concat(result2);
      }
    }, [&] {
      result = diff_seq(xs, ys);
    });
    return result;
  }
  
  container_type sort_seq(container_type& xs) {
    container_type result;
    long n = xs.size();
    parray<value_type> tmp(n);
    xs.backn(tmp.begin(), n);
    xs.clear();
    if (n > 1) {
      std::sort(tmp.begin(), tmp.end());
    }
    long i = 0;
    for (auto it = tmp.cbegin(); it != tmp.cend(); it++, i++) {
      value_type v = *it;
      if (i > 0 && same_key(v, result.back())) {
        continue;
      }
      result.push_back(v);
    }
    return result;
  }
  
  class merge_output {
  public:
    
    using result_type = container_type;
    using array_type = parray<result_type>;
    using const_iterator = typename array_type::const_iterator;
    
    void init(result_type& dst) const {
      
    }
    
    void copy(const result_type& src, result_type& dst) const {
      dst = src;
    }
    
    void merge(result_type& src, result_type& dst) const {
      dst = std::move(merge_par(src, dst));
    }
    
    void merge(const_iterator lo, const_iterator hi, result_type& dst) const {
      for (const_iterator it = lo; it != hi; it++) {
        merge(*it, dst);
      }
    }
    
  };
  
  container_type sort(container_type& xs) {
    using input_type = level4::chunkedseq_input<container_type>;
    using output_type = merge_output;
    container_type id;
    input_type in(xs);
    output_type out;
    container_type result;
    auto convert_reduce_comp = [&] (input_type& in) {
      return in.seq.size(); // later: use correct value
    };
    auto convert_reduce = [&] (input_type& in, container_type& dst) {
      dst = std::move(sort_seq(in.seq));
    };
    auto seq_convert_reduce = convert_reduce;
    level4::reduce(in, out, id, result, convert_reduce_comp, convert_reduce, seq_convert_reduce);
    return result;
  }
  
  void init() {
    it = seq.begin();
  }
  
  void uniqify() {
    seq = sort(seq);
    init();
  }
  
public:
  
  pset() {
    init();
  }
  
  ~pset() {
    clear();
  }
  
  pset(const pset& other) {
    chunked::copy_dst(other.seq.cbegin(), other.seq.cend(), seq);
    init();
  }
  
  pset(pset&& other)
  : seq(std::move(other.seq)) { }
  
  pset(std::initializer_list<value_type> xs)
  : seq(xs) {
    uniqify();
  }
  
  template <class Iter>
  pset(Iter lo, Iter hi) {
    chunked::copy_dst(lo, hi, seq);
    uniqify();
  }
  
  pset(long sz, const std::function<value_type(long)>& body) {
    chunked::tabulate_dst(sz, seq, [&] (long i, reference dst) {
      dst = body(i);
    });
    uniqify();
  }
  
  pset(long sz,
       const std::function<long(long)>& body_comp,
       const std::function<value_type(long)>& body) {
    chunked::tabulate_dst(sz, seq, body_comp, [&] (long i, reference dst) {
      dst = body(i);
    });
    uniqify();
  }
  
  size_type size() const {
    return seq.size();
  }
  
  bool empty() const {
    return size() == 0;
  }
  
  iterator find(const key_type& k) {
    iterator it = first_larger_or_eq(k);
    return (same_key(*it, k)) ? it : seq.end();
  }
  
  const_iterator find(const key_type& k) const {
    const_iterator it = first_larger_or_eq(k);
    return (same_key(*it, k)) ? it : seq.end();
  }
  
  std::pair<iterator,bool> insert(const value_type& val) {
    bool already = false;
    it = first_larger_or_eq(val);
    if (it == seq.end()) {
      // val is currently the largest item in the container
      seq.push_back(val);
    } else if (same_key(*it, val)) {
      // val is present in the container
      already = true;
    } else {
      // val is currently not yet present in the container
      it = seq.insert(it, val);
    }
    return std::make_pair(it, already);
  }
  
  void erase(iterator it) {
    if (it == seq.end()) {
      return;
    }
    if (it + 1 == seq.end()) {
      seq.pop_back();
      return;
    }
    seq.erase(it, it+1);
  }
  
  size_type erase(const key_type& k) {
    size_type nb = seq.size();
    erase(find(k));
    return nb - seq.size();
  }
  
  iterator begin() const {
    return seq.begin();
  }
  
  iterator end() const {
    return seq.end();
  }
  
  const_iterator cbegin() const {
    return seq.cbegin();
  }
  
  const_iterator cend() const {
    return seq.cend();
  }
  
  void check() const {
    seq.check();
  }
  
  void merge(pset& other) {
    seq = merge(seq, other.seq);
    other.clear();
  }
  
  void intersect(pset& other) {
    seq = intersect(seq, other.seq);
    other.clear();
  }
  
  void diff(pset& other) {
    seq = diff(seq, other.seq);
    other.clear();
  }
  
  void clear() {
    chunked::clear(seq);
  }
  
};

/***********************************************************************/

} // end namespace
} // end namespace

#endif /*! _PCTL_PSET_BASE_H_ */
