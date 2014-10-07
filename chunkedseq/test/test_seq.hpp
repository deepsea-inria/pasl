/*!
 * \author Umut A. Acar
 * \author Arthur Chargueraud
 * \author Mike Rainey
 * \date 2013-2018
 * \copyright 2013 Umut A. Acar, Arthur Chargueraud, Mike Rainey
 *
 * \brief Unit tests for sequences
 * \file test_seq.hpp
 *
 */

#include "cmdline.hpp"


#ifndef _PASL_TEST_SEQ_H_
#define _PASL_TEST_SEQ_H_

template <class Seq, class Item, class ItemGenerator = Item>
class TestSeq {
public:

  /*---------------------------------------------------*/
  // Auxiliary function for iterated push

  static void push(Seq& s, int nb, int offset) { 
    for (int i = 0; i < nb; i++) {
      int j = offset+i;
      Item x = ItemGenerator::from_int(j);
      s.push_back(x); 
    }
  }

  /*---------------------------------------------------*/
  // Auxiliary function for printing

  static void print_seq(Seq& s) {
    s.template print<ItemGenerator>(); 
    printf("\n");
  }

  /*---------------------------------------------------*/

  enum fifo_or_lifo { lifo, fifo };

  template<fifo_or_lifo mode>
  static void test_pushpop() {
    size_t nb = (size_t) pasl::util::cmdline::parse_or_default_int64("nb", 35);
    Seq s;
    printf("------------\nStarting\n");
    for (int i = 0; i < nb; i++) {
      printf("-------------\nPushing front %d\n", i);
      s.push_front(ItemGenerator::from_int(i));
      print_seq(s);
      s.check();
    }
    std::cout << "-----" << std::endl;
    std::cout << "size=" << s.size() << std::endl;
    print_seq(s);
    Seq xxx = s;
    print_seq(xxx);
    std::cout << "-----" << std::endl;
    for (int i = nb-1; i >= 0; i--) {
      print_seq(s);
      printf("-------------\nPopping %s %d\n", ((lifo) ? "front" : "back"), i);
      Item r;
      if (mode == lifo)
        r = s.pop_front();
      else
        r = s.pop_back();
      printf("  =%d\n", ItemGenerator::to_int(r));
      ItemGenerator::free(r);
      s.check();
    }
  }

  /*---------------------------------------------------*/

  static void test_concat() {
    Seq s;
    Seq t;
    size_t nbs = (size_t) pasl::util::cmdline::parse_or_default_int64("nbs", 2);
    size_t nbt = (size_t) pasl::util::cmdline::parse_or_default_int64("nbt", 5);
    push(s, nbs, 0);
    print_seq(s);
    push(t, nbt, nbs);
    print_seq(t);
    s.concat(t);
    printf("ok\n");
    print_seq(s);
    printf("ok\n");
    s.check();
  }

  /*---------------------------------------------------*/

  static void test_split() {
    Seq s;
    size_t nb = (size_t) pasl::util::cmdline::parse_or_default_int64("nb", 32);
    for (size_t i = 0; i <= nb; i++) {
      printf("======= Splitting at %lu ======\n", i);
      Seq t;
      Seq u;
      //Seq::debug_alloc();
      push(t, nb, 0);
      print_seq(t);
      //Seq::debug_alloc();
      t.split(i, u);
      int sz = t.size();
      //Seq::debug_alloc();
      print_seq(t);
      print_seq(u);
      assert(sz == i);
    }
  }

  /*---------------------------------------------------*/
  /*
  static void test_split_index() {
    Seq s;
    size_t nb = (size_t) pasl::util::cmdline::parse_or_default_int64("nb", 32);
    size_t pos = (size_t) pasl::util::cmdline::parse_or_default_int64("pos", 9);
    for (int i = 0; i < nb; i++) {
      Seq t;
      Seq u;
      //Seq::debug_alloc();
      push(t, nb, 0);
      print_seq(t);
      //Seq::debug_alloc();
      t.split(pos, u);
      //Seq::debug_alloc();
      print_seq(t);
      print_seq(u);
      printf("=============\n");
    }
  }
  */

  /*---------------------------------------------------*/

  /*
  static void test_split_weight() {
    Seq s;
    int nb = 32;
    size_t pos = 9;
    for (int i = 0; i < nb; i++) {
      Seq t;
      Seq u;
      //Seq::debug_alloc();
      push(t, nb, 0);
      print_seq(t);
      //Seq::debug_alloc();
      t.split_by_index(u, pos);
      //Seq::debug_alloc();
      print_seq(t);
      printf("=============\n= %d\n", u.pop_front());
      print_seq(u);
      printf("=============\n");
  }
  */

  /*---------------------------------------------------*/

  // requires commenting out BOOTCHUNKSEQ_TEST if nb is large (e.g. 125)

  static void test_split_concat() {
    size_t nb = (size_t) pasl::util::cmdline::parse_or_default_int64("nb", 15);
    for (int i = 1; i < nb; i++) {
      printf("%d\n", i);
      Seq t;
      Seq u;
      push(t, i, 0);
      for (size_t pos = 0; pos < i; pos++) {
        printf("*****========Splitting %lu=====*****\n", pos);
        t.split(pos, u);
        // print_seq(t);
        // print_seq(u);
        printf("*****========Concatenating %lu=====*****\n", pos);
        t.concat(u);
        // print_seq(t);
        assert(t.size() == i);
        t.check();
      }
      // print_seq(t);
    }  
  }

  /*---------------------------------------------------*/

  using thunk_t = std::function<void ()>;

  static void execute_test() {
    pasl::util::cmdline::argmap<thunk_t> c;
    c.add("push_pop_lifo",  [&]() { test_pushpop<lifo>(); });
    c.add("push_pop_fifo", [&]() { test_pushpop<fifo>(); });
    c.add("concat", [&]() { test_concat(); });
    c.add("split", [&]() { test_split(); });
    c.add("split_concat", [&]() { test_split_concat(); });
    pasl::util::cmdline::dispatch_by_argmap_with_default_all(c, "only");

  }

  /*---------------------------------------------------*/

}; // end class

#endif /*! _PASL_TEST_SEQ_H_ */
