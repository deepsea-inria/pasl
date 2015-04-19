/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file bench.cpp
 * \brief Benchmarking driver
 *
 */

#include "benchmark.hpp"
#include "hash.hpp"
#include "dup.hpp"
#include "string.hpp"
#include "sort.hpp"
#include "graph.hpp"
#include "fib.hpp"
#include "mcss.hpp"
#include "numeric.hpp"
#include "exercises.hpp"

/***********************************************************************/

loop_controller_type almost_sorted_sparray_contr("almost_sorted_sparray");

// returns an array that is sorted up to a given number of swaps
sparray almost_sorted_sparray(long s, long n, long nb_swaps) {
  sparray tmp = sparray(n);
  par::parallel_for(almost_sorted_sparray_contr, 0l, n, [&] (long i) {
    tmp[i] = (value_type)i;
  });
  for (long i = 0; i < nb_swaps; i++)
    std::swap(tmp[random_index(2*i, n)], tmp[random_index(2*i+1, n)]);
  return tmp;
}

loop_controller_type exp_dist_sparray_contr("exp_dist_sparray");

// returns an array with exponential distribution of size n using seed s
sparray exp_dist_sparray(long s, long n) {
  sparray tmp = sparray(n);
  int lg = log2_up(n)+1;
  par::parallel_for(exp_dist_sparray_contr, 0l, n, [&] (long i) {
    long range = (1 << (random_index(2*(i+s), lg)));
    tmp[i] = (value_type)hash64shift((long)(range+random_index(2*(i+s), range)));
  });
  return tmp;
}

/*---------------------------------------------------------------------*/
/* Benchmark framework */

using thunk_type = std::function<void ()>;

using benchmark_type =
  std::pair<std::pair<thunk_type,thunk_type>,
            std::pair<thunk_type, thunk_type>>;

benchmark_type make_benchmark(thunk_type init, thunk_type bench,
                              thunk_type output, thunk_type destroy) {
  return std::make_pair(std::make_pair(init, bench),
                        std::make_pair(output, destroy));
}

void bench_init(const benchmark_type& b) {
  b.first.first();
}

void bench_run(const benchmark_type& b) {
  b.first.second();
}

void bench_output(const benchmark_type& b) {
  b.second.first();
}

void bench_destroy(const benchmark_type& b) {
  b.second.second();
}

/*---------------------------------------------------------------------*/
/* Benchmark definitions */

benchmark_type fib_bench() {
  long n = pasl::util::cmdline::parse_or_default_long("n", 38);
  long* result = new long;
  auto init = [=] {

  };
  auto bench = [=] {
    *result = fib(n);
  };
  auto output = [=] {
    std::cout << "result " << *result << std::endl;
  };
  auto destroy = [=] {
    delete result;
  };
  return make_benchmark(init, bench, output, destroy);
}

// TODO: take threshold as argument, if != -1, set it into
// threshold (which should be renamed mfib_threshold)
// and run mfib; merging the code below with the one above.
benchmark_type mfib_bench() {
  long n = pasl::util::cmdline::parse_or_default_long("n", 38);
  long* result = new long;
  auto init = [=] {
    
  };
  auto bench = [=] {
    *result = mfib(n);
  };
  auto output = [=] {
    std::cout << "result " << *result << std::endl;
  };
  auto destroy = [=] {
    delete result;
  };
  return make_benchmark(init, bench, output, destroy);
}

benchmark_type map_incr_bench(bool student_soln = false) {
  long n = pasl::util::cmdline::parse_or_default_long("n", 1l<<20);
  sparray* inp = new sparray(0);
  sparray* outp = new sparray(0);
  auto init = [=] {
    *inp = fill(n, 1);
  };
  auto bench = [=] {
    sparray& in = *inp;
    if (student_soln) {
      *outp = sparray(in.size());
      exercises::map_incr(&in[0], &(*outp)[0], in.size());
    } else {
      *outp = map([&] (value_type x) { return x+1; }, in);
    }
  };
  auto output = [=] {
    std::cout << "result " << (*outp)[outp->size()-1] << std::endl;
  };
  auto destroy = [=] {
    delete inp;
    delete outp;
  };
  return make_benchmark(init, bench, output, destroy);
}

benchmark_type duplicate_bench(bool ex = false) {
  long n = pasl::util::cmdline::parse_or_default_long("n", 1l<<20);
  sparray* inp = new sparray(0);
  sparray* outp = new sparray(0);
  auto init = [=] {
    *inp = fill(n, 1);
  };
  auto bench = [=] {
    *outp = (ex) ? exercises::duplicate(*inp) : duplicate(*inp);
  };
  auto output = [=] {
    std::cout << "result " << (*outp)[outp->size()-1] << std::endl;
  };
  auto destroy = [=] {
    delete inp;
    delete outp;
  };
  return make_benchmark(init, bench, output, destroy);
}

benchmark_type ktimes_bench(bool ex = false) {
  long n = pasl::util::cmdline::parse_or_default_long("n", 1l<<20);
  long k = pasl::util::cmdline::parse_or_default_long("k", 4);
  sparray* inp = new sparray(0);
  sparray* outp = new sparray(0);
  auto init = [=] {
    *inp = fill(n, 1);
  };
  auto bench = [=] {
    *outp = (ex) ? exercises::ktimes(*inp, k) : ktimes(*inp, k);
  };
  auto output = [=] {
    std::cout << "result " << (*outp)[outp->size()-1] << std::endl;
  };
  auto destroy = [=] {
    delete inp;
    delete outp;
  };
  return make_benchmark(init, bench, output, destroy);
}

using reduce_bench_type = enum { reduce_normal, reduce_max_ex, reduce_plus_ex, reduce_ex };

benchmark_type reduce_bench(reduce_bench_type t = reduce_normal) {
  long n = pasl::util::cmdline::parse_or_default_long("n", 1l<<20);
  sparray* inp = new sparray(0);
  value_type* result = new value_type;
  auto init = [=] {
    *inp = fill(n, 1);
  };
  auto bench = [=] {
    if (t == reduce_normal)
      *result = sum(*inp);
    else if (t == reduce_max_ex)
      *result = exercises::max(&(*inp)[0], inp->size());
    else if (t == reduce_plus_ex)
      *result = exercises::plus(&(*inp)[0], inp->size());
    else if (t == reduce_ex)
      *result = exercises::reduce(plus_fct, 0l, &(*inp)[0], inp->size());
  };
  auto output = [=] {
    std::cout << "result " << *result << std::endl;
  };
  auto destroy = [=] {
    delete inp;
    delete result;
  };
  return make_benchmark(init, bench, output, destroy);
}

benchmark_type scan_bench() {
  long n = pasl::util::cmdline::parse_or_default_long("n", 1l<<20);
  sparray* inp = new sparray(0);
  sparray* outp = new sparray(0);
  auto init = [=] {
    *inp = fill(n, 1);
  };
  auto bench = [=] {
    *outp = prefix_sums_excl(*inp).partials;
  };
  auto output = [=] {
    std::cout << "result " << (*outp)[outp->size()-1] << std::endl;
  };
  auto destroy = [=] {
    delete inp;
    delete outp;
  };
  return make_benchmark(init, bench, output, destroy);
}

benchmark_type filter_bench() {
  long n = pasl::util::cmdline::parse_or_default_long("n", 1l<<20);
  sparray* inp = new sparray(0);
  sparray* outp = new sparray(0);
  auto init = [=] {
    *inp = gen_random_sparray(n);
  };
  auto bench = [=] {
    *outp = exercises::filter(is_even_fct, *inp);
  };
  auto output = [=] {
    std::cout << "result " << (*outp)[outp->size()-1] << std::endl;
  };
  auto destroy = [=] {
    delete inp;
    delete outp;
  };
  return make_benchmark(init, bench, output, destroy);
}

benchmark_type mcss_bench() {
  long n = pasl::util::cmdline::parse_or_default_long("n", 1l<<20);
  sparray* inp = new sparray(0);
  value_type* outp = new value_type;
  auto init = [=] {
    *inp = gen_random_sparray(n);
  };
  auto bench = [=] {
    *outp = mcss(*inp);
  };
  auto output = [=] {
    std::cout << "result " << *outp << std::endl;
  };
  auto destroy = [=] {
    delete inp;
    delete outp;
  };
  return make_benchmark(init, bench, output, destroy);
}

benchmark_type dmdvmult_bench() {
  long n = pasl::util::cmdline::parse_or_default_long("n", 4000);
  long nxn = n*n;
  sparray* mtxp = new sparray(0);
  sparray* vecp = new sparray(0);
  sparray* outp = new sparray(0);
  auto init = [=] {
    *mtxp = gen_random_sparray(nxn);
    *vecp = gen_random_sparray(n);
  };
  auto bench = [=] {
    *outp = dmdvmult(*mtxp, *vecp);
  };
  auto output = [=] {
    std::cout << "result " << (*outp)[outp->size()-1] << std::endl;
  };
  auto destroy = [=] {
    delete mtxp;
    delete vecp;
    delete outp;
  };
  return make_benchmark(init, bench, output, destroy);
}

benchmark_type sort_bench() {
  long n = pasl::util::cmdline::parse_or_default_long("n", 1l<<20);
  sparray* inp = new sparray(0);
  sparray* outp = new sparray(0);
  pasl::util::cmdline::argmap<std::function<sparray (sparray&)>> algos;
  algos.add("quicksort",          [] (sparray& xs) { return quicksort(xs); });
  algos.add("mergesort",          [] (sparray& xs) { return mergesort(xs); });
  algos.add("mergesort_seqmerge", [] (sparray& xs) { return mergesort<false>(xs); });
  algos.add("cilksort",           [] (sparray& xs) { return cilksort(xs); });
  algos.add("mergesort_ex",       [] (sparray& xs) { return exercises::mergesort(xs); });
  auto sort_fct = algos.find_by_arg("bench");
  auto init = [=] {
    pasl::util::cmdline::argmap_dispatch c;
    c.add("random", [=] {
      *inp = gen_random_sparray(n);
    });
    c.add("almost_sorted", [=] {
      long nb_swaps = pasl::util::cmdline::parse_or_default_long("nb_swaps", 1000);
      *inp = almost_sorted_sparray(1232, n, nb_swaps);
    });
    c.add("exponential_dist", [=] {
      *inp = exp_dist_sparray(12323, n);
    });
    c.find_by_arg_or_default_key("generator", "random")();
  };
  auto bench = [=] {
    *outp = sort_fct(*inp);
  };
  auto output = [=] {
    std::cout << "result " << (*outp)[outp->size()-1] << std::endl;
  };
  auto destroy = [=] {
    delete inp;
    delete outp;
  };
  return make_benchmark(init, bench, output, destroy);
}

benchmark_type graph_bench() {
  adjlist* graphp = new adjlist;
  sparray* visitedp = new sparray;
  std::string fname = pasl::util::cmdline::parse_or_default_string("fname", "");
  vtxid_type source = pasl::util::cmdline::parse_or_default_long("source", (value_type)0);
  pasl::util::cmdline::argmap<std::function<sparray (const adjlist&, vtxid_type)>> algos;
  algos.add("bfs", [&] (const adjlist& graph, vtxid_type source) { return bfs(graph, source); });
  algos.add("bfs_ex", [&] (const adjlist& graph, vtxid_type source) { return exercises::bfs(graph, source); });
  auto bfs_fct = algos.find_by_arg("bench");
  if (fname == "")
    pasl::util::atomic::fatal([] { std::cerr << "missing filename for graph: -fname filename"; });
  auto init = [=] {
    graphp->load_from_file(fname);
  };
  auto bench = [=] {
    //*visitedp = bfs(*graphp, source);
    *visitedp = bfs_fct(*graphp, source);
  };
  auto output = [=] {
    long nb_visited = sum(*visitedp);
    long max_dist = max(*visitedp);
    std::cout << "nb_visited\t" << nb_visited << std::endl;
    std::cout << "max_dist\t" << max_dist << std::endl;
  };
  auto destroy = [=] {
    delete graphp;
    delete visitedp;
  };
  return make_benchmark(init, bench, output, destroy);
}

/*---------------------------------------------------------------------*/
/* PASL Driver */

int main(int argc, char** argv) {

  benchmark_type bench;
  
  auto init = [&] {
    pasl::util::cmdline::argmap<std::function<benchmark_type()>> m;
    m.add("fib",                  [&] { return fib_bench(); });
    m.add("mfib",                 [&] { return mfib_bench(); });
    m.add("map_incr",             [&] { return map_incr_bench(); });
    m.add("reduce",               [&] { return reduce_bench(); });
    m.add("scan",                 [&] { return scan_bench(); });
    m.add("mcss",                 [&] { return mcss_bench(); });
    m.add("dmdvmult",             [&] { return dmdvmult_bench(); });
    m.add("quicksort",            [&] { return sort_bench(); });
    m.add("mergesort",            [&] { return sort_bench(); });
    m.add("mergesort_seqmerge",   [&] { return sort_bench(); });
    m.add("cilksort",             [&] { return sort_bench(); });
    m.add("graph",                [&] { return graph_bench(); });
    m.add("duplicate",            [&] { return duplicate_bench(); });
    m.add("ktimes",               [&] { return ktimes_bench(); });
    

    m.add("map_incr_ex",          [&] { return map_incr_bench(true); });
    m.add("sum_ex",               [&] { return reduce_bench(reduce_plus_ex); });
    m.add("max_ex",               [&] { return reduce_bench(reduce_max_ex); });
    m.add("reduce_ex",            [&] { return reduce_bench(reduce_ex); });
    m.add("duplicate_ex",         [&] { return duplicate_bench(true); });
    m.add("ktimes_ex",            [&] { return ktimes_bench(true); });
    m.add("filter_ex",            [&] { return filter_bench(); });
    m.add("mergesort_ex",         [&] { return sort_bench(); });
    
    bench = m.find_by_arg("bench")();
    bench_init(bench);
  };
  auto run = [&] (bool) {
    bench_run(bench);
  };
  auto output = [&] {
    bench_output(bench);
  };
  auto destroy = [&] {
    bench_destroy(bench);
  };
  pasl::sched::launch(argc, argv, init, run, output, destroy);
}

/***********************************************************************/

