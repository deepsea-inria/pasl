#include <iostream>
#include "fixedcapacity.hpp"
#include "algebra.hpp"
#include "cachedmeasure.hpp"
#include "chunk.hpp"
#include "bootchunkedseq.hpp"
#include "itemsearch.hpp"

using namespace pasl::data;



!!!!!!!!
DEPRECATED
!!!!!!!!









template <class Item, class Cache, int Chunk_capacity>
class bootchunkseq_deque_config {
public:
  
  using size_type = size_t;
  using value_type = Item;
  
  using client_cache_type = Cache;
  using cache_type = client_cache_type;
  
  using heap_allocator_type = fixedcapacity::inline_allocator<value_type, Chunk_capacity>;
  template <class Ringbuffer_item>
  using ringbuffer_type = fixedcapacity::ringbuffer_ptr<heap_allocator_type>;
  
  using chunk_annotation_type = struct { };
  
  template <class Chunk_item, class Chunk_cache>
  using chunk_gen_type = chunk<ringbuffer_type<Chunk_item>, Chunk_cache, chunk_annotation_type>;
  
};

template <class Item, int Capacity>
using cdeque = bootchunkseq<bootchunkseq_deque_config<Item, cachedmeasure::trivial<Item, size_t>, Capacity>>;

int main(int argc, const char * argv[])
{
  using value_type = int;
  using heap_allocator = fixedcapacity::heap_allocator<value_type, 4>;
  using vector_type = fixedcapacity::stack<heap_allocator>;
  using cache_type = cachedmeasure::size<value_type, size_t>;
  using measure_type = typename cache_type::measure_type;
  using annotation_type = struct { };
  using chunk_type = chunk<vector_type, cache_type, annotation_type>;
  
  measure_type meas;
  
  chunk_type c;
  
  c.push_back(meas, 123);
  
  std::cout << c.cached << std::endl;
  
  
  cdeque<int, 4> cd;
  cd.clear();
  
  return 0;
}

