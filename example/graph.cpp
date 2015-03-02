#include <iostream>
#include "benchmark.hpp"
#include "parray.hpp"

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
    

  }
}

long sum(pa::parray<long>& xs) {
  return pa::reduce(xs, 0l, [&] (long x, long y) {
    return x + y;
  });
}

int main(int argc, char **argv) {
 
  auto init = [&] {
    
  };
  auto run = [&] (bool) {
    /*
    pa::parray<long> foo = { 1, -1, 1, 3 };
    std::cout << foo << std::endl;
    std::cout << sum(foo) << std::endl;
    
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
