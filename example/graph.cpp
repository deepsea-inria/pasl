#include <iostream>
#include "benchmark.hpp"
#include "parray.hpp"
#include "pchunkedseq.hpp"

#include <functional>

namespace pa = pasl::pctl::parray;

namespace pasl {
  namespace pctl {
    namespace parray {
      template <class Item>
      std::ostream& operator<<(std::ostream& out, const parray<Item>& xs) {
        out << "{ ";
        size_t sz = xs.size();
        for (long i = 0; i < sz; i++) {
          out << xs[i];
          if (i+1 < sz)
            out << ", ";
        }
        out << " }";
        return out;
      }
      
      
      
    }
    
    namespace pchunkedseq {
      template <class Item>
      std::ostream& operator<<(std::ostream& out, const pchunkedseq<Item>& xs) {
        return pasl::data::chunkedseq::extras::generic_print_container(out, xs.seq);
      }
    }

  }
}

long sum(pa::parray<long>& xs) {
  return pasl::pctl::reduce(xs.begin(), xs.end(), 0l, [&] (long x, long y) {
    return x + y;
  });
}

int main(int argc, char **argv) {
 
  auto init = [&] {
    
  };
  auto run = [&] (bool) {
  

    pa::parray<int> foo = { 1, 2, 3, 4, 5 };
    std::cout << foo << std::endl;
    
    pa::parray<int> foo23 = pasl::pctl::scan(foo.cbegin(), foo.cend(), 0, [&] (int x, int y) {
      return x +y;
    }, pasl::pctl::exclusive_scan);
    
    std::cout << "foo=" << foo23 << std::endl;

    /*
    pasl::pctl::pchunkedseq::pchunkedseq<long> pc;
    pc.seq.push_back(3);
    pc.seq.push_front(343);
    pasl::pctl::pchunkedseq::pchunkedseq<long> pc2 = std::move(pc);
    std::cout << pc << std::endl;
    std::cout << pc2 << std::endl;
*/
    
    pasl::pctl::pchunkedseq::pchunkedseq<long> pc(30, [&] (long i) {
      return 2*i;
    });
    std::cout << "pc = " << pc << std::endl;

    pasl::pctl::pchunkedseq::pchunkedseq<long> pc2 = { 3433, 33, 12} ;
    std::cout << "pc2 = " << pc2 << std::endl;
    
    /*
    pa::parray<long> foobar(14, [&] (long i) {
      return i+1;
    });
    std::cout << foobar << std::endl;
*/
    
    
    pa::parray<long> xs = pasl::pctl::weights(15, [&] (long x) { return 1; });
    std::cout << "weights(15) = " << xs << std::endl;
    pasl::pctl::parallel_for(0l, xs.size(), [&] (long x) { return 1; }, [&] (long i) {
      xs[i]++;
    });
    
    std::cout << "xs=" <<  xs << std::endl;
     
  };
  auto output = [&] {
  };
  auto destroy = [&] {
  };
  pasl::sched::launch(argc, argv, init, run, output, destroy);
}
