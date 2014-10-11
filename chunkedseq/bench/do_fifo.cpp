
#define BOOTNEW 1

#include <assert.h>

#include "cmdline.hpp"
#include "atomic.hpp"
#include "microtime.hpp"

#include "container.hpp"
#include "fixedcapacity.hpp"
#include "chunkedseq.hpp"
#include "cachedmeasure.hpp"

#ifdef USE_MALLOC_COUNT
#include "malloc_count.h"
#endif


using namespace pasl;
using namespace pasl::util;
namespace chunkedseq = pasl::data::chunkedseq;

/***********************************************************************/

typedef size_t result_t;


result_t res = 0;
double exec_time;

/*---------------------------------------------------------------------*/

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


/*---------------------------------------------------------------------*/
/* Scenarios */


// #define FIFO_LIFO_ONLY_COUNT_POP 1

template <class Datastruct>
void scenario_lifo() {
  typedef typename Datastruct::value_type value_type;
  size_t nb_total = (size_t) cmdline::parse_or_default_int64("n", 100000000);
  size_t repeat = (size_t) cmdline::parse_or_default_int64("r", 1000);
  size_t block = nb_total / repeat;
  printf("length %lld\n",block);
  uint64_t start_time = microtime::now();
  Datastruct d;
  res = 0;
  for (size_t j = 0; j < repeat; j++) {
    for (size_t i = 0; i < block; i++) {
      d.push_back(value_type(i));
      res += i;
    }
    for (size_t i = 0; i < block; i++) {
      res += d.pop_back().get();
    }
  }
  exec_time = microtime::seconds_since(start_time);
}

template <class Datastruct>
void scenario_fifo() {
  typedef typename Datastruct::value_type value_type;
  size_t nb_total = (size_t) cmdline::parse_or_default_int64("n", 100000000);
  size_t repeat = (size_t) cmdline::parse_or_default_int64("r", 1000);
  size_t block = nb_total / repeat;
  printf("length %lld\n",block);
  uint64_t start_time = microtime::now();
  Datastruct d;
  res = 0;
  for (size_t j = 0; j < repeat; j++) {
    for (size_t i = 0; i < block; i++) {
      d.push_back(value_type(i));
      res += i;
    }
   for (size_t i = 0; i < block; i++) {
      res += d.pop_front().get();
    }
  }
  exec_time = microtime::seconds_since(start_time);
}



/*---------------------------------------------------------------------*/
// dispatch sequences

using Item = bytes_8;


using seq1_type = pasl::data::stl::deque_seq<Item>;

using seq2_type = chunkedseq::bootstrapped::deque<
  Item,
  512,
  pasl::data::cachedmeasure::trivial<Item, size_t>,
  data::fixedcapacity::heap_allocated::ringbuffer_ptr >;

using fftree_deque_type = chunkedseq::ftree::deque<
  Item,
  512,
  pasl::data::cachedmeasure::trivial<Item, size_t>,
  data::fixedcapacity::heap_allocated::ringbuffer_ptr >;

#if 0

using seq3_type = chunkedseq::bootstrapped::deque<
  Item,
  512,
  pasl::data::cachedmeasure::trivial<Item, size_t>,
  data::fixedcapacity::heap_allocated::ringbuffer_ptrx >;
#endif


int main(int argc, char** argv) {
  pasl::util::cmdline::set(argc, argv);
  res = 0;

  std::string sequence = cmdline::parse_or_default_string("sequence", "stl_deque");
  std::string scenario = cmdline::parse_or_default_string("scenario", "fifo");
  int chunk_size = cmdline::parse_or_default_int("chunk_size", 512);
  if (chunk_size != 512) {
    printf("not valid chunk size\n");
    exit(1);
  }

  if (scenario == "fifo") {
    if (sequence == "stl_deque") {
      scenario_fifo<seq1_type>();
    } else
    if (sequence == "chunkedseq") { // ptr
      scenario_fifo<seq2_type>();
#if 0
    } else if (sequence == "chunkedseq_ptrx") {
      scenario_fifo<seq3_type>();
#endif
    } else if (sequence == "chunkedftree") {
      scenario_fifo<fftree_deque_type>();
    } else {
      printf("not valid sequence\n");
      exit(1);
    }
  }
  else if (scenario == "lifo") {
    if (sequence == "stl_deque") {
      scenario_lifo<seq1_type>();
    } else
    if (sequence == "chunkedseq") {
      scenario_lifo<seq2_type>();
    #if 0
    } else if (sequence == "chunkedseq_ptrx") {
      scenario_fifo<seq3_type>();
    #endif
    } else if (sequence == "chunkedftree") {
      scenario_fifo<fftree_deque_type>();
    } else {
      printf("not valid sequence\n");
      exit(1);
    }
  }

  printf ("exectime %lf\n", exec_time);
  printf("result %lld\n", (long long)res);
  #ifdef USE_MALLOC_COUNT
  malloc_pasl_report();
  #endif

  return 0;
}




/***********************************************************************/
