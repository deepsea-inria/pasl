/*!
 * \author Umut A. Acar
 * \author Arthur Chargueraud
 * \author Mike Rainey
 * \date 2013-2018
 * \copyright 2013 Umut A. Acar, Arthur Chargueraud, Mike Rainey
 *
 * \brief Unit tests for chunkedseq
 * \file fftree.cpp
 *
 */

// #define DEBUG_MIDDLE_SEQUENCE 1


#include "cmdline.hpp"
#include "chunkedseq.hpp"
#include "test_seq.hpp"

using namespace pasl::data;


/***********************************************************************/
// Specification of integer items

class IntItem {
  int value;
  
public:
  IntItem() : value(0) {}

  static IntItem from_int(int n) {
    IntItem x;
    x.value = n;
    return x;
  }

  static int to_int(IntItem& x) { 
    return x.value;
  }

  static void print(IntItem& x) {
    printf("%d", IntItem::to_int(x));
  }

  static void free(IntItem& x) { 
  }

};

/***********************************************************************/
// Specialization of the sequence to integer items

template<int Chunk_capacity>
using IntSeqOf = chunkedseq::deque<IntItem, Chunk_capacity>;


/***********************************************************************/

int main(int argc, char** argv) {
  pasl::util::cmdline::set(argc, argv);

  size_t chunk_capacity = (size_t) pasl::util::cmdline::parse_or_default_int("chunk_capacity", 2);
  if (chunk_capacity == 2)
    TestSeq<IntSeqOf<2>,IntItem>::execute_test();
  else if (chunk_capacity == 4)
    TestSeq<IntSeqOf<4>,IntItem>::execute_test();
  else
    pasl::util::cmdline::die("unsupported capacity");

  return 0;
}


/***********************************************************************/
