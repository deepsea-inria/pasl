/*!
 * \author Umut A. Acar
 * \author Arthur Chargueraud
 * \author Mike Rainey
 * \date 2013-2018
 * \copyright 2014 Umut A. Acar, Arthur Chargueraud, Mike Rainey
 *
 * \brief Benchmarking for chunked sequences
 * \file fftree.cpp
 *
 */


#include <functional>
#include <random>
#include <algorithm>
#include <assert.h>
#include <unordered_map>

#include "cmdline.hpp"
#include "atomic.hpp"
#include "microtime.hpp"

// "ls_bag.hpp"
#include "container.hpp"
#include "fixedcapacity.hpp"
#include "chunkedseq.hpp"
#include "chunkedbag.hpp"
#include "map.hpp"

#ifdef USE_MALLOC_COUNT
#include "malloc_count.h"
#endif

using namespace pasl;
using namespace pasl::util;
namespace chunkedseq = pasl::data::chunkedseq;

/***********************************************************************/

typedef size_t result_t;

typedef std::function<void ()> thunk_t;

//! \todo see cmdline invalid argument 
void failwith(std::string s) {
  std::cout << s << std::endl;
  exit(1);
}

result_t res = 0;
double exec_time;

/*---------------------------------------------------------------------*/

/*
 * Pseudo-random generator defined by the congruence S' = 69070 * S
 * mod (2^32 - 5).  Marsaglia (CACM July 1993) says on page 107 that
 * this is a ``good one''.  There you go.
 *
 * The literature makes a big fuss about avoiding the division, but
 * for us it is not worth the hassle.
 */
static const unsigned RNGMOD = ((1ULL << 32) - 5);
static const unsigned RNGMUL = 69070U;
unsigned rand_seed;

unsigned myrand() {
  unsigned state = rand_seed;
  state = (unsigned)((RNGMUL * (unsigned long long)state) % RNGMOD);
  rand_seed = state;
  return state;
}

void mysrand(unsigned seed) {
  seed %= RNGMOD;
  seed += (seed == 0); /* 0 does not belong to the multiplicative
                        group.  Use 1 instead */
  rand_seed = seed;
}

/*---------------------------------------------------------------------*/

class bytes_1 {
public:
  char data;
  bytes_1() { }
  bytes_1(char c) : data(c) { }
  bytes_1(size_t i) {
    data = (char) (i % 256);
  }
  size_t get() const {
    return (size_t) data;
  }
  char get_char() const {
    return data;
  }
  bool operator==(const bytes_1 other) {
    return data == other.data;
  }
  operator unsigned char() const {
    return data;
  }
};

class bytes_8 {
public:
  uint64_t data;
  bytes_8() { }
  bytes_8(char c) {
    data = (uint64_t)c;
  }
  bytes_8(size_t i) {
    data = i;
  }
  size_t get() const {
    return data;
  }
  char get_char() const {
    return (char) (data % 256);
  }
  bool operator==(const bytes_8 other) {
    return data == other.data;
  }
};

class bytes_64 {
public:
  int64_t data[8];
  bytes_64() { }
  bytes_64(char c) {
    for (int k = 0; k < 8; k++)
      data[k] = (int64_t)c;
  }
  bytes_64(size_t i) {
    for (int k = 0; k < 8; k++)
      data[k] = i;
  }
  size_t get() const {
    return data[0];
  }
  char get_char() const {
    return (char) (data[0] % 256);
  }
  bool operator==(const bytes_64 other) {
    return data == other.data;
  }
};

/* reorders in a random fashion the items of the given container
 */
template <class Datastruct>
void shuffle(Datastruct& d) {
  size_t sz = d.size();
  for (size_t i = 0; i < sz; i++)
    std::swap(d[i], d[rand() % sz]);
}

/*---------------------------------------------------------------------*/
/* Scenarios */


template <class Datastruct>
thunk_t scenario_test() {
  typedef typename Datastruct::value_type value_type;
  size_t nb_total = (size_t) cmdline::parse_or_default_int64("n", 100000000);
  size_t repeat = (size_t) cmdline::parse_or_default_int64("r", 1000);
  size_t block = nb_total / repeat;
  return [=] {
    printf("length %lld\n",block);
    uint64_t start_time = microtime::now();
    Datastruct d;
    Datastruct r;
    res = 0;
    for (size_t j = 0; j < repeat; j++) {
      for (size_t i = 0; i < block; i++) {
        d.push_back(value_type(i));
        res += i;
      }
     for (size_t i = 0; i < block; i++) {
        res += d.pop_front().get();
        d.push_back(value_type(i));
        size_t v = d.pop_front().get();
        r.push_back(value_type(v));
      }
    }
    exec_time = microtime::seconds_since(start_time);
  };
}


// #define FIFO_LIFO_ONLY_COUNT_POP 1

/* Common code for various lifo versions */
//! \todo factorize fifo and lifo
template <class Datastruct, bool SkipPop>
thunk_t scenario_lifo_with_or_without_pop() {
  typedef typename Datastruct::value_type value_type;
  size_t nb_total = (size_t) cmdline::parse_or_default_int64("n", 100000000);
  size_t repeat = (size_t) cmdline::parse_or_default_int64("r", 1000);
  size_t block = nb_total / repeat;
  return [=] {
    printf("length %lld\n",block);
    uint64_t start_time = microtime::now();
    Datastruct d;
    res = 0;
    for (size_t j = 0; j < repeat; j++) {
      for (size_t i = 0; i < block; i++) {
        d.push_back(value_type(i));
        res += i;
      }
      #ifdef FIFO_LIFO_ONLY_COUNT_POP
      start_time = microtime::now();
      #endif
      if (!SkipPop) {
        for (size_t i = 0; i < block; i++) {
          res += d.pop_back().get();
        }
      }
    }
    // if (SkipPop)
    //  res += d.size();
    exec_time = microtime::seconds_since(start_time);
  };
}

template <class Datastruct>
thunk_t scenario_lifo() {
  return scenario_lifo_with_or_without_pop<Datastruct, false>();
}

template <class Datastruct>
thunk_t scenario_fill_back() {
  return scenario_lifo_with_or_without_pop<Datastruct, true>();
}

template <class Datastruct>
thunk_t scenario_fifo() {
  typedef typename Datastruct::value_type value_type;
  size_t nb_total = (size_t) cmdline::parse_or_default_int64("n", 100000000);
  size_t repeat = (size_t) cmdline::parse_or_default_int64("r", 1000);
  size_t block = nb_total / repeat;
  return [=] {
    printf("length %lld\n",block);
    uint64_t start_time = microtime::now();
    Datastruct d;
    res = 0;
    for (size_t j = 0; j < repeat; j++) {
      for (size_t i = 0; i < block; i++) {
        d.push_back(value_type(i));
        res += i;
      }
      #ifdef FIFO_LIFO_ONLY_COUNT_POP
      start_time = microtime::now();
      #endif
     for (size_t i = 0; i < block; i++) {
        res += d.pop_front().get();
      }
    }
    exec_time = microtime::seconds_since(start_time);
  };
}


// p should be 2 or more
template <class Datastruct, bool should_push, bool should_pop>
void _scenario_split_merge(Datastruct* ds, size_t n, size_t p, size_t r, size_t h) {
  typedef typename Datastruct::value_type value_type;
  typedef typename Datastruct::size_type size_type;
  
  size_t nb_total = n / p;
  printf("length %lld\n",nb_total);
  
  srand(14);
  for (size_type i = 0; i < p; i++) {
    for (size_type j = 0; j < nb_total; j++)
      ds[i].push_back(value_type((char)1));
  }
  uint64_t start_time = microtime::now();
  for (size_type k = 0; k < r; k++) {
    size_type b1 = rand() % p;
    size_type b2 = rand() % (p-1);
    if (b2 >= b1)
      b2++;
    // b1 != b2
    ds[b1].concat(ds[b2]);
    // ds[b2].empty() == true
    size_type b3 = rand() % (p-1);
    if (b3 >= b2)
      b3++;
    // b3 != b2
    Datastruct& src = ds[b3];
    if (src.size() > 1) {
      size_t posn = src.size() / 2;
      src.split(posn, ds[b2]);
    }
    if (should_push)
      for (size_t i = 0; i < h; i++)
        src.push_back(value_type(i));
    if (should_pop)
      for (size_t i = 0; i < h; i++)
        res += (size_type)src.pop_front().get();
  }
  exec_time = microtime::seconds_since(start_time);
  res = 0;
  for (int i = 0; i < p; i++)
    res += ds[i].size();
}

template <class Datastruct>
thunk_t scenario_split_merge() {
  typedef typename Datastruct::value_type value_type;
  typedef typename Datastruct::size_type size_type;
  size_t n = (size_t) cmdline::parse_or_default_int64("n", 100000000);
  size_t p = (size_t) cmdline::parse_or_default_int64("p", std::max(n/100, (size_type)2));
  size_t r = (size_t) cmdline::parse_or_default_int64("r", 100000);
  size_t h = (size_t) cmdline::parse_or_default_int64("h", 0);
  bool should_push = cmdline::parse_or_default_bool("should_push", true);
  bool should_pop = cmdline::parse_or_default_bool("should_pop", false);
  
  return [=] {
    Datastruct* ds = new Datastruct[p];
    if (should_push && should_pop)
      _scenario_split_merge<Datastruct,true,true>(ds, n, p, r, h);
    else if (! should_push && should_pop)
      _scenario_split_merge<Datastruct,false,true>(ds, n, p, r, h);
    else if (should_push && ! should_pop)
      _scenario_split_merge<Datastruct,true,false>(ds, n, p, r, h);
    else
      _scenario_split_merge<Datastruct,false,false>(ds, n, p, r, h);
    delete [] ds;
  };
}

template <class Datastruct, class Filter>
void filter(Datastruct& dst, Datastruct& src, const Filter& filt, int cutoff) {
  typedef typename Datastruct::value_type value_type;
  if (src.size() <= cutoff) {
    while (! src.empty()) {
      value_type item = src.pop_back();
      if (filt(item))
        dst.push_back(item);
    }
  } else {
    Datastruct src2;
    Datastruct dst2;
    size_t mid = src.size() / 2;
    src.split(mid, src2);
    filter(dst,  src,  filt, cutoff);
    filter(dst2, src2, filt, cutoff);
    dst.concat(dst2);
  }
}

template <class Datastruct>
thunk_t scenario_filter() {
  typedef typename Datastruct::size_type size_type;
  typedef typename Datastruct::value_type value_type;
  size_t cutoff = cmdline::parse_or_default_int64("cutoff", 8096);
  size_t n = cmdline::parse_or_default_int64("n", 100000000);
  size_t r = (size_t) cmdline::parse_or_default_int64("r", 1);
  size_t nb_total = n / r;
  printf("length %lld\n",nb_total);
  const size_t m = 1<<30;
  return [=] {
    Datastruct src;
    Datastruct dst;
    for (size_t i = 0; i < nb_total; i++)
      src.push_back(value_type(i));
    auto filt = [] (value_type v) {
      return (v.get() % m) != 0;
    };
    uint64_t start_time = microtime::now();
    for (size_t i = 0; i < r; i++) {
      filter(dst, src, filt, cutoff);
      dst.swap(src);
    }
    exec_time = microtime::seconds_since(start_time);
    res = src.size() + dst.size();
    //assert(dst.size() == n - ((n+m-1)/m));
  };
}

#ifndef SKIP_MAP

/* All of these dictionary benchmarks are taken from:
 * http://tommyds.sourceforge.net/doc/benchmark.html
 */
#define PAYLOAD 16 // Size of the object

template <class Map,class Obj>
thunk_t scenario_map() {
  using map_type = Map;
  using key_type = typename map_type::key_type;
  using obj_type = Obj;
  using mapped_type = typename map_type::mapped_type;
  
  size_t n = cmdline::parse_or_default_uint64("n", 1000000);
  bool test_random = cmdline::parse_or_default_bool("test_random", true);
  key_type* INSERT = new key_type[n];
  key_type* SEARCH = new key_type[n];
  obj_type* OBJ = new obj_type[n];
  
  // Initialize the keys
  for(long i=0;i<n;++i) {
    INSERT[i] = 0x80000000 + i * 2;
    SEARCH[i] = 0x80000000 + i * 2;
  }
  
  // If random order is required, shuffle the keys with Fisher-Yates
  // The two key orders are not correlated
  if (test_random) {
    std::random_shuffle(INSERT, INSERT + n);
    std::random_shuffle(SEARCH, SEARCH + n);
  }

  auto init = [=] (map_type& bag) {
    for(size_t i=0;i<n;++i) {
      // Setup the element to insert
      key_type key = INSERT[i];
      auto element = &OBJ[i];
      element->value = key;
      
      // Insert it
      bag[key] = element;
    }
  };
  
  cmdline::argmap_dispatch c;
  c.add("insert", [=] {
    map_type bag;
    uint64_t start_time = microtime::now();
    init(bag);
    exec_time = microtime::seconds_since(start_time);
    res = bag.size();
  });
  c.add("change", [=] {
    map_type bag;
    init(bag);
    uint64_t start_time = microtime::now();

    for(size_t i=0;i<n;++i) {
      // Search the element
      unsigned key = SEARCH[i];
      auto j = bag.find(key);
      if (j == bag.end())
	abort();
      
      // Remove it
      auto element = (*j).second;
      bag.erase(j);
      
      // Reinsert the element with a new key
      // Use +1 in the key to ensure that the new key is unique
      key = INSERT[i] + 1;
      element->value = key;
      bag[key] = element;
    }
    exec_time = microtime::seconds_since(start_time);
    res = bag.size();
  });
  c.add("hit", [=] {
    map_type bag;
    init(bag);
    uint64_t start_time = microtime::now();
    for(size_t i=0;i<n;++i) {
      // Search the element
      // Use a different key order than insertion
      auto key = SEARCH[i];
      auto j = bag.find(key);
      if (j == bag.end())
        abort();

      // Ensure that it's the correct element.
      // This operation is like using the object after finding it,
      // and likely involves a cache-miss operation.
      auto element = (*j).second;
      if (element->value != key)
        abort();
    }
    exec_time = microtime::seconds_since(start_time);
    res = bag.size();
  });
  c.add("miss", [=] {
    map_type bag;
    init(bag);
    uint64_t start_time = microtime::now();
    for(size_t i=0;i<n;++i) {
      // Search the element
      key_type key = SEARCH[i]+1;
      auto j = bag.find(key);
      /*      if (j != bag.end())
	      abort(); */
    }
    exec_time = microtime::seconds_since(start_time);
    res = bag.size();
  });
  c.add("remove", [=] {
    map_type bag;
    init(bag);
    uint64_t start_time = microtime::now();
    for(size_t i=0;i<n;++i) {
      // Search the element
      auto key = SEARCH[i];
      auto j = bag.find(key);
      if (j == bag.end())
        abort();
      
      // Remove it
      bag.erase(j);
      
      // Ensure that it's the correct element.
      auto element = (*j).second;
      /*      if (element->value != key)
	      abort(); */
    }
    exec_time = microtime::seconds_since(start_time);
    res = bag.size();
  });

  return c.find_by_arg("map_benchmark");
}
#endif

/*---------------------------------------------------------------------*/
// dispatch tests

template <class Sequence>
void dispatch_by_scenario() {
  cmdline::argmap_dispatch c;
  c.add("test", scenario_test<Sequence>());
  c.add("fifo", scenario_fifo<Sequence>());
  c.add("lifo", scenario_lifo<Sequence>());
  c.add("fill_back", scenario_fill_back<Sequence>());
  c.add("split_merge", scenario_split_merge<Sequence>());
  c.add("filter", scenario_filter<Sequence>());
  cmdline::dispatch_by_argmap(c, "scenario");
}

/*---------------------------------------------------------------------*/
// dispatch sequences

#include "cachedmeasure.hpp"
#include "fixedcapacity.hpp"
template <
  template <
    class Item,
    int Chunk_capacity=512,          
    class Cache = pasl::data::cachedmeasure::trivial<Item, size_t>,
template<class Chunk_item, int Cap, class Item_alloc2=std::allocator<Item>> class Chunk_struct = pasl::data::fixedcapacity::heap_allocated::ringbuffer_ptr,
    class Item_alloc=std::allocator<Item> >  class SeqStruct,
  class Item, 
  template<class Chunk_item, int Cap, class Item_alloc2=std::allocator<Item>> class Chunk_struct>
void dispatch_for_chunkedseq() {
  using Cache = pasl::data::cachedmeasure::trivial<Item, size_t>;
  static constexpr int default_chunksize = 512;
  util::cmdline::argmap_dispatch c;
  c.add("512",  [] { dispatch_by_scenario<SeqStruct<Item,512,Cache,Chunk_struct> >(); });
  #ifndef SKIP_CHUNKSIZE
  c.add("64",  [] { dispatch_by_scenario<SeqStruct<Item,64,Cache,Chunk_struct> >(); });
  c.add("128",  [] { dispatch_by_scenario<SeqStruct<Item,128,Cache,Chunk_struct> >(); });
  c.add("256",  [] { dispatch_by_scenario<SeqStruct<Item,256,Cache,Chunk_struct> >(); });
  c.add("1024", [] { dispatch_by_scenario<SeqStruct<Item,1024,Cache,Chunk_struct> >(); });
  c.add("2048", [] { dispatch_by_scenario<SeqStruct<Item,2048,Cache,Chunk_struct> >(); });
  c.add("4096", [] { dispatch_by_scenario<SeqStruct<Item,4096,Cache,Chunk_struct> >(); });
  c.add("8192", [] { dispatch_by_scenario<SeqStruct<Item,8192,Cache,Chunk_struct> >(); });
  #endif
  util::cmdline::dispatch_by_argmap(c, "chunk_size", std::to_string(default_chunksize));
}


template <class Item,
int Chunk_capacity=512,
class Cache=data::cachedmeasure::trivial<Item, size_t>,
template<class Chunk_item, int Cap, class Item_alloc2=std::allocator<Item>> class Chunk_struct=data::fixedcapacity::heap_allocated::ringbuffer_ptr,
class Item_alloc=std::allocator<Item> >
using mystack = chunkedseq::bootstrapped::stack<Item, Chunk_capacity, Cache>;

template <class Item,
int Chunk_capacity=512,
class Cache=data::cachedmeasure::trivial<Item, size_t>,
template<class Chunk_item, int Cap, class Item_alloc2=std::allocator<Item>> class Chunk_struct=data::fixedcapacity::heap_allocated::ringbuffer_ptr,
class Item_alloc=std::allocator<Item> >
using mybag = chunkedseq::bootstrapped::bagopt<Item, Chunk_capacity, Cache>;


template <class Item,
int Chunk_capacity=512,
class Cache=data::cachedmeasure::trivial<Item, size_t>,
template<class Chunk_item, int Cap, class Item_alloc2=std::allocator<Item>> class Chunk_struct=data::fixedcapacity::heap_allocated::ringbuffer_ptr,
class Item_alloc=std::allocator<Item> >
using myfftreestack = chunkedseq::ftree::stack<Item, Chunk_capacity, Cache>;

template <class Item,
int Chunk_capacity=512,
class Cache=data::cachedmeasure::trivial<Item, size_t>,
template<class Chunk_item, int Cap, class Item_alloc2=std::allocator<Item>> class Chunk_struct=data::fixedcapacity::heap_allocated::ringbuffer_ptr,
class Item_alloc=std::allocator<Item> >
using myfftreebag = chunkedseq::ftree::bagopt<Item, Chunk_capacity, Cache>;

template <class Item>
void dispatch_by_sequence() {
  util::cmdline::argmap_dispatch c;
  #ifndef SKIP_DEQUE
    c.add("stl_deque", [] {
      using seq_type = pasl::data::stl::deque_seq<Item>;
      dispatch_by_scenario<seq_type>();
    });
  #endif
  #ifndef SKIP_ROPE
  #ifdef HAVE_ROPE
    c.add("stl_rope", [] {
      using seq_type = pasl::data::stl::rope_seq<Item>;
      dispatch_by_scenario<seq_type>();
    });
  #endif
  #endif
  #ifndef SKIP_CHUNKEDSEQ
    c.add("chunkedseq", [] {
      dispatch_for_chunkedseq<chunkedseq::bootstrapped::deque, Item, data::fixedcapacity::heap_allocated::ringbuffer_ptr>();
    });
  #endif
  #ifndef SKIP_CHUNKEDSEQ_OPT
    c.add("chunkedseq_stack", [] {
      dispatch_for_chunkedseq<mystack, Item, data::fixedcapacity::heap_allocated::stack>();
    });
     c.add("chunkedseq_bag", [] {
       dispatch_for_chunkedseq<mybag, Item, data::fixedcapacity::heap_allocated::stack>();
    });
 #endif
  
#ifndef SKIP_FTREE
  c.add("chunkedftree", [] {
    dispatch_for_chunkedseq<chunkedseq::ftree::deque, Item, data::fixedcapacity::heap_allocated::ringbuffer_ptr>();
  });
#endif
#ifndef SKIP_FTREE_OPT
  c.add("chunkedftree_stack", [] {
    dispatch_for_chunkedseq<myfftreestack, Item, data::fixedcapacity::heap_allocated::stack>();
  });
  c.add("chunkedftree_bag", [] {
    dispatch_for_chunkedseq<myfftreebag, Item, data::fixedcapacity::heap_allocated::stack>();
  });
#endif
  util::cmdline::dispatch_by_argmap(c, "sequence");
}

/*---------------------------------------------------------------------*/
// dispatch itemsize
  
void dispatch_by_itemsize() {
  static constexpr int default_itemsize = 8;
  util::cmdline::argmap_dispatch c;
  c.add("8",  [] { dispatch_by_sequence<bytes_8>(); });
  #ifndef SKIP_ITEMSIZE
  c.add("1",  [] { dispatch_by_sequence<bytes_1>(); });
  c.add("64", [] { dispatch_by_sequence<bytes_64>(); });
  #endif
  util::cmdline::dispatch_by_argmap(c, "itemsize", std::to_string(default_itemsize));
}

/*---------------------------------------------------------------------*/
// dispatch maps

#ifndef SKIP_MAP
void dispatch_by_map() {
  cmdline::argmap_dispatch c;
  using key_type = long;
  using obj_type = struct {
    key_type value;
    char payload[PAYLOAD];
  };
  using value_type = obj_type*;
  using stl_map_type = std::map<key_type, value_type>;
  using chunkedseq_map_type = data::map::map<key_type, value_type>;
  using unordered_map_type = std::unordered_map<key_type, value_type>;
  c.add("stl_map", scenario_map<stl_map_type,obj_type>());
  c.add("chunkedseq_map", scenario_map<chunkedseq_map_type,obj_type>());
  c.add("stl_unordered_set", scenario_map<unordered_map_type,obj_type>());
  cmdline::dispatch_by_argmap(c, "map");
}
#else
void dispatch_by_map() {
  abort();
}
#endif


/*---------------------------------------------------------------------*/

void dispatch_by_benchmark_mode() {
  cmdline::argmap_dispatch c;
  c.add("sequence", [] { dispatch_by_itemsize(); });
  c.add("map",      [] { dispatch_by_map(); });
  cmdline::dispatch_by_argmap(c, "mode", "sequence");
}

int main(int argc, char** argv) {
  pasl::util::cmdline::set(argc, argv);
  mysrand(233432432);
  res = 0;
  
  dispatch_by_benchmark_mode();
  
  printf ("exectime %lf\n", exec_time);
  printf("result %lld\n", (long long)res);

  #ifdef USE_MALLOC_COUNT
  malloc_pasl_report();
  #endif
  return 0;
}




/***********************************************************************/
