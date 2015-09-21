#include "benchmark.hpp"
#include "granularity.hpp"
#include "hash.hpp"
#include "static-contract-functions.hpp"
//}

/***************************************************************************/

int main (int argc, char** argv) {

  long n; // number of vertices
  double f; // chain factor
  long seed;
  bool print;
  contraction::hash::Forest* F;
  contraction::hash::Forest* result;

  auto init = [&] {
    n = std::max(2l, pasl::util::cmdline::parse_or_default_long("n", 10l));
    f = pasl::util::cmdline::parse_or_default_double("f", 0.5);
    seed = pasl::util::cmdline::parse_or_default_long("seed", 42l);
    print = pasl::util::cmdline::parse_or_default_bool("print", false);

    F = contraction::hash::blank_forest(n);
    contraction::hash::initialize_empty(F);

    std::cout << "Generating forest... " << std::flush;

    long b = contraction::hash::MAX_DEGREE - 1;
    long r = std::max(n - lround(f * (double) n), 2l);
    for (long i = 1; i < r; i++) {
      // flip a coin to determine if edge is inserted
      if (contraction::hash::heads(i, i/b)) contraction::hash::insert_edge(F, i, i/b);
    }

    srand(seed);
    for (long i = r; i < n; i++) {
      // pick a random edge
      long v = rand() % i;
      while (contraction::hash::degree(F, v) == 0) v = rand() % i;
      long u = contraction::hash::ith_neighbor(F, v, rand() % contraction::hash::degree(F, v));

      // replace this edge with two edges
      contraction::hash::delete_edge(F, v, u);
      contraction::hash::insert_edge(F, v, i);
      contraction::hash::insert_edge(F, i, u);
    }

    // check how many vertices have degree 2
    long count = 0;
    for (long v = 0; v < n; v++) {
      if (contraction::hash::degree(F, v) == 2) count++;
    }
    std::cout << "done. " << (((double) count) / ((double) n) * 100.0)
              << "% of vertices have degree 2." << std::endl;

    if (print) contraction::hash::display_forest(F);

  };

  auto run = [&] (bool sequential) {
    result = contraction::hash::forest_contract(F);
  };

  auto output = [&] {
    if (print) contraction::hash::display_forest(result);
  };

  auto destroy = [&] {
    ;
  };

  pasl::sched::launch(argc, argv, init, run, output, destroy);

  return 0;
}
