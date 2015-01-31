#include <iostream>
#include "benchmark.hpp"
#include "parray.hpp"

namespace pa = pasl::data::parray;

int main(int argc, char **argv) {
  
  auto init = [&] {
    
  };
  auto run = [&] (bool) {
    pa::parray<long> foo = { 1, -1, 1, 3 };
    std::cout << foo << std::endl;
    std::cout << pa::sum(foo) << std::endl;
  };
  auto output = [&] {
  };
  auto destroy = [&] {
  };
  pasl::sched::launch(argc, argv, init, run, output, destroy);
}
