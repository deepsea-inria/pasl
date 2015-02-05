/*!
 * \author Umut A. Acar
 * \author Arthur Chargueraud
 * \author Mike Rainey
 * \date 2013-2018
 * \copyright 2014 Umut A. Acar, Arthur Chargueraud, Mike Rainey
 *
 * \brief Unit tests for our chunked sequence
 * \file quickcheck_chunkedseq.cpp
 *
 */

#include "cmdline.hpp"
#include "properties.hpp"

/***********************************************************************/

namespace pasl {
namespace data {
  
/*---------------------------------------------------------------------*/

static constexpr int default_capacity = 8;
  
int nb_tests;
bool print_chunkedseq_verbose;
bool generate_by_insert;
    
template <class Property>
void checkit(std::string msg) {
  assert(nb_tests > 0);
  quickcheck::check<Property>(msg.c_str(), nb_tests);
}

/*---------------------------------------------------------------------*/
/* Unit tests for sequence data structures */
  
template <class Properties>
void chunkedseq_dispatch_by_property() {
  util::cmdline::argmap_dispatch c;
  c.add("pushpop", [] { // run push pop tests
    auto msg = "we get consistent results on randomly selected "
               "sequences of pushes and pops";
    checkit<typename Properties::push_pop_sequence_same>(msg);
  });
  c.add("split", [] { // run split tests
    auto msg = "we get consistent results for calls to split on a "
               "random position in the sequence";
    checkit<typename Properties::split_same>(msg);
  });
  c.add("split_in_range", [] { // run split-in-range tests
    auto msg = "we get consistent results for calls to split on "
               "a range of valid positions";
    checkit<typename Properties::split_in_range_same>(msg);
  });
  c.add("concat", [] { // run concat tests
    auto msg = "we get consistent results on calls to concat";
    checkit<typename Properties::concat_same>(msg);
  });
  c.add("random_access", [] {
    auto msg = "we get consistent results on random accesses to the "
               "container";
    checkit<typename Properties::random_access_same>(msg);
  });
  c.add("iterator", [] {
    auto msg = "we get consistent results on iterator-based traversal";
    checkit<typename Properties::iterator_same>(msg);
  });
  c.add("random_access_iterator", [] {
    auto msg = "we get consistent results on random iterator-based traversal";
    checkit<typename Properties::random_access_iterator_same>(msg);
  });
  c.add("insert", [] {
    auto msg = "we get consistent results over calls to insert";
    checkit<typename Properties::insert_same>(msg);
  });
  c.add("erase", [] {
    auto msg = "we get consistent results over calls to erase";
    checkit<typename Properties::erase_same>(msg);
  });
  c.add("for_each_segment", [] {
    auto msg = "we get correct results over calls to for_each_segment";
    checkit<typename Properties::for_each_segment_correct>(msg);
  });
  c.add("for_each_in_interval", [] {
    auto msg = "we get correct results over calls to for_each_segment on random intervals";
    checkit<typename Properties::for_each_in_interval_correct>(msg);
  });
  c.add("pushn_popn", [] {
    auto msg = "we get correct results over calls to pushn and to popn";
    checkit<typename Properties::pushn_popn_sequence_same>(msg);
  });
  c.add("backn_frontn", [] {
    auto msg = "we get correct results over calls to backn and frontn";
    checkit<typename Properties::backn_frontn_sequence_same>(msg);
  });
  print_dashes();
  util::cmdline::dispatch_by_argmap_with_default_all(c, "property");
  print_dashes();
}
  
using value_type = int;
using trusted_sequence_container_type = pasl::data::stl::deque_seq<value_type>;
using trusted_bag_container_type = trusted_sequence_container_type;
template <class Untrusted_container>
class container_copy_from_untrusted_to_trusted {
public:
  static trusted_sequence_container_type conv(const Untrusted_container& u) {
    trusted_sequence_container_type t;
    u.for_each([&] (value_type v) {
      t.push_back(v);
    });
    return t;
  }
};
template <class Untrusted_sequence_container>
  using sequence_container_pair = container_pair<trusted_sequence_container_type, Untrusted_sequence_container, container_copy_from_untrusted_to_trusted<Untrusted_sequence_container>>;
template <class Untrusted_sequence_container>
using sequence_container_properties = chunkseqproperties<sequence_container_pair<Untrusted_sequence_container>>;
  
template <int Chunk_capacity>
void seq_dispatch_by_container() {
  util::cmdline::argmap_dispatch c;
  c.add("chunked_bootstrapped_deque", [] {
    using deque_type = chunkedseq::bootstrapped::deque<value_type, Chunk_capacity>;
    chunkedseq_dispatch_by_property<sequence_container_properties<deque_type>>();
  });
  c.add("chunked_bootstrapped_stack", [] {
    using deque_type = chunkedseq::bootstrapped::stack<value_type, Chunk_capacity>;
    chunkedseq_dispatch_by_property<sequence_container_properties<deque_type>>();
  });
#ifndef SKIP_NON_DEQUE
  c.add("chunked_ftree_deque", [] {
    using deque_type = chunkedseq::ftree::deque<value_type, Chunk_capacity>;
    chunkedseq_dispatch_by_property<sequence_container_properties<deque_type>>();
  });
  c.add("chunked_ftree_stack", [] {
    using deque_type = chunkedseq::ftree::stack<value_type, Chunk_capacity>;
    chunkedseq_dispatch_by_property<sequence_container_properties<deque_type>>();
  });
  /*
  c.add("triv", [] {
    using deque_type = bootchunkedseq::triv<value_type, Chunk_capacity>;
    chunkedseq_dispatch_by_property<container_properties<deque_type>>();
  });
   */
#endif
  util::cmdline::dispatch_by_argmap_with_default_all(c, "datastruct");
}
  
void seq_dispatch_by_capacity() {
  util::cmdline::argmap_dispatch c;
  c.add("2",   [] { seq_dispatch_by_container<2>(); });
  c.add("8",   [] { seq_dispatch_by_container<8>(); });
  c.add("512", [] { seq_dispatch_by_container<512>(); });
  util::cmdline::dispatch_by_argmap(c, "chunk_capacity", std::to_string(default_capacity));
}
  
/*---------------------------------------------------------------------*/
/* Unit tests for bag data structures */

template <class Properties>
void chunkedbag_dispatch_by_property() {
  util::cmdline::argmap_dispatch c;
  c.add("pushpop", [] { // run push pop tests
    auto msg = "we get consistent results on randomly selected "
    "sequences of pushes and pops";
    checkit<typename Properties::push_pop_sequence_same>(msg);
  });
  c.add("split", [] { // run split tests
    auto msg = "we get consistent results for calls to split on a "
    "random position in the sequence";
    checkit<typename Properties::split_same>(msg);
  });
  c.add("concat", [] { // run concat tests
    auto msg = "we get consistent results on calls to concat";
    checkit<typename Properties::concat_same>(msg);
  });
  c.add("iterator", [] {
    auto msg = "we get consistent results on iterator-based traversal";
    checkit<typename Properties::iterator_same>(msg);
  });
  c.add("for_each_segment", [] {
    auto msg = "we get correct results over calls to for_each_segment";
    checkit<typename Properties::for_each_segment_correct>(msg);
  });
  c.add("pushn_popn", [] {
    auto msg = "we get correct results over calls to pushn and to popn";
    checkit<typename Properties::pushn_popn_sequence_same>(msg);
  });
  c.add("backn_frontn", [] {
    auto msg = "we get correct results over calls to backn and frontn";
    checkit<typename Properties::backn_frontn_sequence_same>(msg);
  });
  print_dashes();
  util::cmdline::dispatch_by_argmap_with_default_all(c, "property");
  print_dashes();
}
  
template <class Untrusted_bag_container>
using bag_container_pair = container_pair<trusted_bag_container_type, Untrusted_bag_container, container_copy_from_untrusted_to_trusted<Untrusted_bag_container>, bag_container_same<trusted_bag_container_type>>;
template <class Untrusted_bag_container>
using bag_container_properties = chunkedbagproperties<bag_container_pair<Untrusted_bag_container>>;
  
template <int Chunk_capacity>
void bag_dispatch_by_container() {
  util::cmdline::argmap_dispatch c;
  c.add("chunked_bootstrapped", [] {
    using bag_type = chunkedseq::bootstrapped::bagopt<value_type, Chunk_capacity>;
    chunkedbag_dispatch_by_property<bag_container_properties<bag_type>>();
  });
#ifndef SKIP_NON_DEQUE
  c.add("chunked_ftree_bag", [] {
    using deque_type = chunkedseq::ftree::bagopt<value_type, Chunk_capacity>;
    chunkedbag_dispatch_by_property<bag_container_properties<deque_type>>();
  });
#endif
  util::cmdline::dispatch_by_argmap_with_default_all(c, "datastruct");
}
  
void bag_dispatch_by_capacity() {
  util::cmdline::argmap_dispatch c;
  c.add("2",   [] { bag_dispatch_by_container<2>(); });
  c.add("8",   [] { bag_dispatch_by_container<8>(); });
  c.add("512", [] { bag_dispatch_by_container<512>(); });
  util::cmdline::dispatch_by_argmap(c, "chunk_capacity", std::to_string(default_capacity));
}
  
/*---------------------------------------------------------------------*/
/* Unit tests for dynamic dictionary */

using trusted_map_type = std::map<int,int>;
using untrusted_map_type = map::map<int, int>;
class map_copy_from_untrusted_to_trusted {
public:
  static trusted_map_type conv(const untrusted_map_type& u) {
    trusted_map_type t;
    for (auto it = u.begin(); it != u.end(); it++) {
      auto p = *it;
      t[p.first] = p.second;
    }
    return t;
  }
};
using map_container_pair = container_pair<trusted_map_type, untrusted_map_type, map_copy_from_untrusted_to_trusted>;
using map_properties = mapproperties<map_container_pair>;

void map_dispatch() {
  auto msg = "we get consistent results with std::map";
  checkit<typename map_properties::map_same>(msg);
}
  
/*---------------------------------------------------------------------*/
    
} // end namespace
} // end namespace

int main(int argc, char** argv) {
  pasl::util::cmdline::set(argc, argv);
  pasl::data::nb_tests = pasl::util::cmdline::parse_or_default_int("nb_tests", 1000);
  pasl::data::print_chunkedseq_verbose = pasl::util::cmdline::parse_or_default_bool("verbose", true);
  pasl::data::generate_by_insert = pasl::util::cmdline::parse_or_default_bool("generate_by_insert", false);
  pasl::util::cmdline::argmap_dispatch c;
  c.add("sequence",           [] { pasl::data::seq_dispatch_by_capacity(); });
  c.add("bag",                [] { pasl::data::bag_dispatch_by_capacity(); });
  c.add("map",                [] { pasl::data::map_dispatch(); });
  pasl::util::cmdline::dispatch_by_argmap(c, "profile", "sequence");
  return 0;
}


/***********************************************************************/
