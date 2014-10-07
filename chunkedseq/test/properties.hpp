/*!
 * \author Umut A. Acar
 * \author Arthur Chargueraud
 * \author Mike Rainey
 * \date 2013-2018
 * \copyright 2014 Umut A. Acar, Arthur Chargueraud, Mike Rainey
 *
 * \brief Testing properties to be used by randomize unit testing to check invariants
 * \file properties.hpp
 *
 */

#include "generators.hpp"

#ifndef _PASL_DATA_TEST_PROPERTIES_H_
#define _PASL_DATA_TEST_PROPERTIES_H_

namespace pasl {
namespace data {
  
/***********************************************************************/

/*---------------------------------------------------------------------*/
/* Unit test properties for chunked bag */

template <class Bag_pair>
class chunkedbagproperties {
public:
  
  using bag_pair_type = Bag_pair;
  using trusted_type = typename bag_pair_type::trusted_container_type;
  using untrusted_type = typename bag_pair_type::untrusted_container_type;
  using value_type = typename trusted_type::value_type;

  // to check that a random sequence of pushes and pops give consistent results
  class push_pop_sequence_same : public quickcheck::Property<size_t, bag_pair_type> {
  public:
    bool holdsFor(const size_t& _nb_items, const bag_pair_type& _items) {
      bag_pair_type items(_items);
      size_t nb_items = _nb_items;
      random_push_pop_sequence(nb_items, items);
      return check_and_print_container_pair(items);
    }
  };
  
  static bool split_and_check(bag_pair_type& items_src,
                              bag_pair_type& items_dst,
                              size_t split_position) {
    assert(items_src.ok());
    assert(split_position <= items_src.trusted.size());
    items_src.trusted.split(split_position, items_dst.trusted);
    items_src.untrusted.split(split_position, items_dst.untrusted);
    bool ok_src_sz = items_src.trusted.size() == items_src.untrusted.size();
    bool ok_dst_sz = items_dst.trusted.size() == items_dst.untrusted.size();
    items_src.trusted.concat(items_dst.trusted);
    items_src.untrusted.concat(items_dst.untrusted);
    bool ok = check_and_print_container_pair(items_src);
    bool all_ok = ok_src_sz && ok_dst_sz && ok;
    return all_ok;
  }
  
  // to check that a call to split on a random position gives consistent results
  class split_same : public quickcheck::Property<bag_pair_type> {
  public:
    bool holdsFor(const bag_pair_type& _items) {
      size_t sz = _items.trusted.size();
      size_t split_position = quickcheck::generateInRange(size_t(0), sz);
      bag_pair_type items_src(_items);
      bag_pair_type items_dst;
      assert(items_dst.trusted.empty());
      assert(items_dst.untrusted.empty());
      return split_and_check(items_src, items_dst, split_position);
    }
  };
  
  // to check that a concatentation gives consistent results
  class concat_same : public quickcheck::Property<bag_pair_type, bag_pair_type> {
  public:
    bool holdsFor(const bag_pair_type& _items1, const bag_pair_type& _items2) {
      bag_pair_type items1(_items1);
      bag_pair_type items2(_items2);
      items1.trusted.concat(items2.trusted);
      items1.untrusted.concat(items2.untrusted);
      bool items1_ok = check_and_print_container_pair(items1, "items1");
      bool items2_ok = check_and_print_container_pair(items2, "items2");
      return items1_ok && items2_ok;
    }
  };
  
  // to check that iterating via the iterator gives consistent results
  class iterator_same : public quickcheck::Property<bag_pair_type> {
  public:
    int nb = 0;
    bool holdsFor(const bag_pair_type& _items) {
      bag_pair_type items(_items);
      if (items.trusted.size() == 0)
        return true;
      assert(check_and_print_container_pair(items, ""));
      assert(items.untrusted.begin().size() == 1);
      assert(items.untrusted.size() + 1 == items.untrusted.end().size());
      nb++;
      if (nb == 33)
        std::cout << "" << std::endl;
      typename bag_pair_type::trusted_container_type t;
      typename bag_pair_type::trusted_container_type u;
      bool go_forward = flip_coin();
      if (go_forward) { // iterate forward
        for (auto it = items.trusted.begin(); it != items.trusted.end(); it++)
          t.push_back(*it);
        for (auto it = items.untrusted.begin(); it != items.untrusted.end(); it++)
          u.push_back(*it);
      } else { // iterate in reverse
        for (auto it = items.trusted.end() - 1; it != items.trusted.begin(); it--)
          t.push_back(*it);
        for (auto it = items.untrusted.end() - 1; it != items.untrusted.begin(); it--)
          u.push_back(*it);
        t.push_back(*items.trusted.begin());
        u.push_back(*items.untrusted.begin());
      }
      bool ok = bag_pair_type::trusted_same::same(t, u);
      if (! ok) {
        std::cout << "t.size=" <<t.size() << "u.size=" << u.size() << std::endl;
        std::cout << t << std::endl;
        std::cout << u << std::endl;
      }
      return ok;
    }
  };
  
  // to check that the for_each_segment operator gives correct results
  class for_each_segment_correct : public quickcheck::Property<bag_pair_type> {
  public:
    bool holdsFor(const bag_pair_type& _items) {
      bag_pair_type items(_items);
      for (size_t i = 0; i < items.trusted.size(); i++)
        items.trusted[i]++;
      items.untrusted.for_each_segment([&] (value_type* lo, value_type* hi) {
        for (value_type* p = lo; p != hi; p++)
          (*p)++;
      });
      bool ok = check_and_print_container_pair(items);
      return ok;
    }
  };
  
  // to check that multi-push and multi-pop operations give correct results
  class pushn_popn_sequence_same : public quickcheck::Property<bag_pair_type,std::vector<int>> {
  public:
    bool holdsFor(const bag_pair_type& _items, const std::vector<int>& _vec) {
      bag_pair_type items(_items);
      size_t sz_items = items.trusted.size();
      std::vector<int> trusted_vec(_vec);
      std::vector<int> untrusted_vec(_vec);
      size_t sz_vec = _vec.size();
      int* trusted_data_vec = trusted_vec.data();
      int* untrusted_data_vec = untrusted_vec.data();
      bool should_push = flip_coin();
      bool ok1;
      if (should_push) {
        items.trusted.pushn_back(trusted_data_vec, sz_vec);
        items.untrusted.pushn_back(untrusted_data_vec, sz_vec);
        ok1 = check_and_print_container_pair(items);
      } else { // should pop
        size_t nb_to_pop = std::min(sz_items, sz_vec);
        trusted_vec.resize(nb_to_pop);
        untrusted_vec.resize(nb_to_pop);
        items.trusted.popn_back(trusted_data_vec, nb_to_pop);
        items.untrusted.popn_back(untrusted_data_vec, nb_to_pop);
        ok1 = items.trusted.size() == items.untrusted.size();
      }
      bool ok2 = trusted_vec.size() == untrusted_vec.size();
      if (! ok2) {
        std::cout << "  trusted vec:" << trusted_vec << std::endl;
        std::cout << "untrusted vec:" << untrusted_vec << std::endl;
      }
      return ok1 && ok2;
    }
  };
  
  class backn_frontn_sequence_same : public quickcheck::Property<bag_pair_type> {
  public:
    bool holdsFor(const bag_pair_type& _items) {
      bag_pair_type items(_items);
      size_t sz_items = items.trusted.size();
      size_t nb = (size_t)quickcheck::generateInRange(0, (int)sz_items);
      std::vector<int> trusted_vec(nb, -1);
      std::vector<int> untrusted_vec(nb, -1);
      bool act_on_front = flip_coin();
      if (act_on_front) {
        items.trusted.frontn(trusted_vec.data(), nb);
        items.untrusted.frontn(untrusted_vec.data(), nb);
      } else { // read from back
        items.trusted.backn(trusted_vec.data(), nb);
        items.untrusted.backn(untrusted_vec.data(), nb);
      }
      bool ok = trusted_vec.size() == untrusted_vec.size();
      if (! ok) {
        std::cout << "  trusted vec:" << trusted_vec << std::endl;
        std::cout << "untrusted vec:" << untrusted_vec << std::endl;
      }
      return ok;
    }
  };
  
};
  
/*---------------------------------------------------------------------*/
/* Unit test properties for chunked sequence */
  
template <class Container_pair>
class chunkseqproperties {
public:
  
  using container_pair_type = Container_pair;
  using trusted_type = typename container_pair_type::trusted_container_type;
  using untrusted_type = typename container_pair_type::untrusted_container_type;
  using value_type = typename trusted_type::value_type;
  
  // to check that a random sequence of pushes and pops give consistent results
  class push_pop_sequence_same : public quickcheck::Property<size_t, container_pair_type> {
  public:
    bool holdsFor(const size_t& _nb_items, const container_pair_type& _items) {
      container_pair_type items(_items);
      size_t nb_items = _nb_items;
      random_push_pop_sequence(nb_items, items);
      return check_and_print_container_pair(items);
    }
  };
  
  static bool split_and_check(container_pair_type& items_src,
                              container_pair_type& items_dst,
                              size_t split_position) {
    assert(items_src.ok());
    assert(split_position <= items_src.trusted.size());
    items_src.trusted.split(split_position, items_dst.trusted);
    items_src.untrusted.split(split_position, items_dst.untrusted);
    bool src_ok = check_and_print_container_pair(items_src, "src");
    bool dst_ok = check_and_print_container_pair(items_dst, "dst");
    bool all_ok = src_ok && dst_ok;
    if (! all_ok) {
      std::cout << "split position is " << split_position << std::endl;
    }
    return all_ok;
  }
  
  // to check that a call to split on a random position gives consistent results
  class split_same : public quickcheck::Property<container_pair_type> {
  public:
    bool holdsFor(const container_pair_type& _items) {
      size_t sz = _items.trusted.size();
      size_t split_position = quickcheck::generateInRange(size_t(0), sz);
      container_pair_type items_src(_items);
      container_pair_type items_dst;
      assert(items_dst.trusted.empty());
      assert(items_dst.untrusted.empty());
      return split_and_check(items_src, items_dst, split_position);
    }
  };
  
  // to check that calls to split at all legal positions in a randomly selected range gives consistent results
  class split_in_range_same : public quickcheck::Property<container_pair_type> {
  public:
    bool holdsFor(const container_pair_type& _items) {
      size_t sz = _items.trusted.size();
      size_t start = quickcheck::generateInRange(size_t(0), sz);
      size_t end = std::min(sz, start + 10);
      for (size_t split_position = start; split_position <= end; split_position++) {
        container_pair_type items_src(_items);
        container_pair_type items_dst;
        if (! split_and_check(items_src, items_dst, split_position))
          return false;
      }
      return true;
    }
  };
  
  // to check that a concatentation gives consistent results
  class concat_same : public quickcheck::Property<container_pair_type, container_pair_type> {
  public:
    bool holdsFor(const container_pair_type& _items1, const container_pair_type& _items2) {
      container_pair_type items1(_items1);
      container_pair_type items2(_items2);
      items1.trusted.concat(items2.trusted);
      items1.untrusted.concat(items2.untrusted);
      bool items1_ok = check_and_print_container_pair(items1, "items1");
      bool items2_ok = check_and_print_container_pair(items2, "items2");
      return items1_ok && items2_ok;
    }
  };
  
  // to check that the subscript operator gives consistent results
  class random_access_same : public quickcheck::Property<container_pair_type> {
  public:
    bool holdsFor(const container_pair_type& _items) {
      container_pair_type items(_items);
      size_t sz = items.trusted.size();
      size_t i = quickcheck::generateInRange(size_t(0), sz-1);
      value_type t = items.trusted[i];
      value_type u = items.untrusted[i];
      bool ok = t == u;
      if (! ok) {
        std::cout << "trusted[" << i << "]=" << t << std::endl;
        std::cout << "untrusted[" << i << "]=" << u << std::endl;
      }
      return ok;
    }
  };
  
  // to check that iterating via the iterator gives consistent results
  class iterator_same : public quickcheck::Property<container_pair_type> {
  public:
    int nb = 0;
    bool holdsFor(const container_pair_type& _items) {
      container_pair_type items(_items);
      if (items.trusted.size() == 0)
        return true;
      assert(check_and_print_container_pair(items, ""));
      assert(items.untrusted.begin().size() == 1);
      assert(items.untrusted.size() + 1 == items.untrusted.end().size());
      nb++;
      if (nb == 33)
        std::cout << "" << std::endl;
      typename container_pair_type::trusted_container_type t;
      typename container_pair_type::trusted_container_type u;
      bool go_forward = flip_coin();
      if (go_forward) { // iterate forward
        for (auto it = items.trusted.begin(); it != items.trusted.end(); it++)
          t.push_back(*it);
        for (auto it = items.untrusted.begin(); it != items.untrusted.end(); it++)
          u.push_back(*it);
      } else { // iterate in reverse
        for (auto it = items.trusted.end() - 1; it != items.trusted.begin(); it--)
          t.push_back(*it);
        for (auto it = items.untrusted.end() - 1; it != items.untrusted.begin(); it--)
          u.push_back(*it);
      }
      bool ok = t == u;
      if (! ok) {
        std::cout << "t.size=" <<t.size() << "u.size=" << u.size() << std::endl; 
        std::cout << t << std::endl;
        std::cout << u << std::endl;
      }
      return ok;
    }
  };
  
  // to check that backtracking search gives correct results via random repositionings of the iterator
  class random_access_iterator_same : public quickcheck::Property<container_pair_type> {
  public:
    bool holdsFor(const container_pair_type& _items) {
      class itemprinter {
      public:
        static void print(int x) {
          std::cout << x;
        }
      };
      container_pair_type items(_items);
      size_t sz = items.trusted.size();
      typename container_pair_type::trusted_container_type::iterator it_t = items.trusted.begin();
      typename container_pair_type::untrusted_container_type::iterator it_u = items.untrusted.begin();
      const int nb = 50;
      for (int i = 0; i < nb; i++) {
        int cur = (int)it_u.size() - 1;
        int k = quickcheck::generateInRange(0, (int)sz-1);
        int l = k - cur;

        if (l >= 0) {
          it_t += l;
          it_u += l;
        } else {
          it_t -= -1 * l;
          it_u -= -1 * l;
        }
        size_t it_u_sz = it_u.size();
        if (it_u_sz != k + 1)
          return false;
        int sz_t = int(it_t - items.trusted.begin());
        int sz_u = int(it_u.size() - items.untrusted.begin().size());
        if (sz_t != sz_u)
          return false;
        int xu = *it_u;
        int xt = *it_t;
        if (xu != xt) {
          return false;
        }
      }
      return true;
    }
  };
  
  // to check that the insert operator gives consistent results
  class insert_same : public quickcheck::Property<container_pair_type> {
  public:
    int nb=0;
    bool holdsFor(const container_pair_type& _items) {
      container_pair_type items(_items);
      int nb_to_insert = quickcheck::generateInRange(1, 20);
      for (int i = 0; i < nb_to_insert; i++) {
        nb++;
        if (nb == 8) {
          //std::cout << items.untrusted << std::endl;
        }
        //assert(items.untrusted.begin().size()==1);
        int sz = (int)items.trusted.size();
        size_t pos = quickcheck::generateInRange(0, sz-1);
        value_type x = generate_value<value_type>();
        items.trusted.insert(items.trusted.begin() + pos, x);
        items.untrusted.insert(items.untrusted.begin() + pos, x);
        if (! check_and_print_container_pair(items, "insert1")) {
          std::cout << "insert at pos=" << pos << std::endl;
          std::cout << "val=" << x << std::endl;
          //std::cout << "itsz=" << (items.untrusted.begin()+pos).size() << std::endl;
          return false;
        }
      }
      bool ok = check_and_print_container_pair(items, "final result");
      return ok;
    }
  };
  
  // to check that the erase operator gives consistent results
  class erase_same : public quickcheck::Property<container_pair_type> {
  public:
    bool holdsFor(const container_pair_type& _items) {
      container_pair_type items(_items);
      int target_nb_calls_to_erase = quickcheck::generateInRange(1, 100);
      for (int i = 0; i < target_nb_calls_to_erase; i++) {
        int sz = items.trusted.size();
        if (sz == 0)
          break;
        size_t first = (size_t)quickcheck::generateInRange(0, sz-1);
        size_t nb_rem = sz - first;
        size_t last = (size_t)quickcheck::generateInRange(first, first + nb_rem - 1);
        assert(first >= 0);
        assert(first <= last);
        items.trusted.erase(items.trusted.begin() + first, items.trusted.begin() + last);
        items.untrusted.erase(items.untrusted.begin() + first, items.untrusted.begin() + last);
        if (! check_and_print_container_pair(items, "erase")) {
          std::cout << "first=" << first << " last=" << last << std::endl;
          return false;
        }
      }
      bool ok = check_and_print_container_pair(items, "final result");
      return ok;
    }
  };
  
  // to check that the for_each_segment operator gives correct results
  class for_each_segment_correct : public quickcheck::Property<container_pair_type> {
  public:
    bool holdsFor(const container_pair_type& _items) {
      container_pair_type items(_items);
      for (size_t i = 0; i < items.trusted.size(); i++)
        items.trusted[i]++;
      items.untrusted.for_each_segment([&] (value_type* lo, value_type* hi) {
        for (value_type* p = lo; p != hi; p++)
          (*p)++;
      });
      bool ok = check_and_print_container_pair(items);
      return ok;
    }
  };
  
  // to check that the for_each_segment-in-interval operator gives correct results
  class for_each_in_interval_correct : public quickcheck::Property<container_pair_type> {
  public:
    bool holdsFor(const container_pair_type& _items) {
      container_pair_type items(_items);
      int sz = items.trusted.size();
      size_t first = (size_t)quickcheck::generateInRange(0, sz-1);
      size_t nb_rem = sz - first;
      size_t last = (size_t)quickcheck::generateInRange(first, first + nb_rem - 1);
      for (size_t i = first; i <= last; i++)
        items.trusted[i]++;
      auto beg = items.untrusted.begin() + first;
      auto end = items.untrusted.begin() + last + 1;
      items.untrusted.for_each(beg, end, [&] (value_type& p) { p++; });
      bool ok = check_and_print_container_pair(items);
      return ok;
    }
  };
  
  // to check that multi-push and multi-pop operations give correct results
  class pushn_popn_sequence_same : public quickcheck::Property<container_pair_type,std::vector<int>> {
  public:
    bool holdsFor(const container_pair_type& _items, const std::vector<int>& _vec) {
      container_pair_type items(_items);
      size_t sz_items = items.trusted.size();
      std::vector<int> trusted_vec(_vec);
      std::vector<int> untrusted_vec(_vec);
      size_t sz_vec = _vec.size();
      int* trusted_data_vec = trusted_vec.data();
      int* untrusted_data_vec = untrusted_vec.data();
      bool should_push = flip_coin();
      bool act_on_front = flip_coin();
      if (should_push) {
        if (act_on_front) {
          items.trusted.pushn_front(trusted_data_vec, sz_vec);
          items.untrusted.pushn_front(untrusted_data_vec, sz_vec);
        } else { // push on back
          items.trusted.pushn_back(trusted_data_vec, sz_vec);
          items.untrusted.pushn_back(untrusted_data_vec, sz_vec);
        }
      } else { // should pop
        size_t nb_to_pop = std::min(sz_items, sz_vec);
        trusted_vec.resize(nb_to_pop);
        untrusted_vec.resize(nb_to_pop);
        if (act_on_front) {
          items.trusted.popn_front(trusted_data_vec, nb_to_pop);
          items.untrusted.popn_front(untrusted_data_vec, nb_to_pop);
        } else { // pop from back
          items.trusted.popn_back(trusted_data_vec, nb_to_pop);
          items.untrusted.popn_back(untrusted_data_vec, nb_to_pop);
        }
      }
      bool ok1 = check_and_print_container_pair(items);
      bool ok2 = trusted_vec == untrusted_vec;
      if (! ok2) {
        std::cout << "  trusted vec:" << trusted_vec << std::endl;
        std::cout << "untrusted vec:" << untrusted_vec << std::endl;
      }
      return ok1 && ok2;
    }
  };
  
  class backn_frontn_sequence_same : public quickcheck::Property<container_pair_type> {
  public:
    bool holdsFor(const container_pair_type& _items) {
      container_pair_type items(_items);
      size_t sz_items = items.trusted.size();
      size_t nb = (size_t)quickcheck::generateInRange(0, (int)sz_items);
      std::vector<int> trusted_vec(nb, -1);
      std::vector<int> untrusted_vec(nb, -1);
      bool act_on_front = flip_coin();
      if (act_on_front) {
        items.trusted.frontn(trusted_vec.data(), nb);
        items.untrusted.frontn(untrusted_vec.data(), nb);
      } else { // read from back
        items.trusted.backn(trusted_vec.data(), nb);
        items.untrusted.backn(untrusted_vec.data(), nb);
      }
      bool ok = trusted_vec == untrusted_vec;
      if (! ok) {
        std::cout << "  trusted vec:" << trusted_vec << std::endl;
        std::cout << "untrusted vec:" << untrusted_vec << std::endl;
      }
      return ok;
    }
  };
  
};
  
/*---------------------------------------------------------------------*/
  
template <class Container_pair>
class mapproperties {
public:
  
  using container_pair_type = Container_pair;
  using trusted_type = typename container_pair_type::trusted_container_type;
  using untrusted_type = typename container_pair_type::untrusted_container_type;
  using value_type = typename trusted_type::value_type;

  // to check that our implementation of dynamic dictionary gives consistent results with std::map
  class map_same : public quickcheck::Property<container_pair_type> {
  public:
    int nb = 0;
    bool holdsFor(const container_pair_type& _map) {
      container_pair_type map(_map);
      bool ok = true;
      int nb_new = quickcheck::generateInRange(0, 100);
      static constexpr int lo = 0;
      static constexpr int hi = 1<<15;
      for (int i = 0; i < nb_new; i++) {
        int key = quickcheck::generateInRange(lo, hi);
        int val = generate_value<int>();
        if (map.trusted.find(key) != map.trusted.end()) {
          nb++;
          // key should be in map
          auto it1 = map.trusted.find(key);
          auto it2 = map.untrusted.find(key);
          if (it2 == map.untrusted.end())
            return false;
          bool ok2 = (*it1).second == (*it2).second;
          if (! ok2) {
            std::cout << "trusted=" << (*it1).first << " " << (*it1).second << " untrusted=" << (*it2).first << " " << (*it2).second << std::endl;
            assert(check_and_print_container_pair(map));
            std::cout << map << std::endl;
            return false;
          }
          ok = ok && ok2;
        }
        map.trusted[key] = val;
        map.untrusted[key] = val;
      }
      bool ok2 = check_and_print_container_pair(map);
      if (! ok) {
        return false;
      }
      if (!ok2) {
        return false;
      }
      ok = ok && ok2;
      return ok;
    }
  };
  
};
  
/***********************************************************************/
  
} // end namespace
} // end namespace

#endif /*! _PASL_DATA_TEST_PROPERTIES_H_ */
