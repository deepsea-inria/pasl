/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file example.cpp
 * \brief Example applications
 *
 */

#include <math.h>
#include <thread>

#include "benchmark.hpp"
#include "dup.hpp"
#include "string.hpp"
#include "sort.hpp"
#include "graph.hpp"
#include "mcss.hpp"
#include "numeric.hpp"
#include "exercises.hpp"

/***********************************************************************/

/*---------------------------------------------------------------------*/
/* Examples from text by chapter */

void map_incr(const long* source, long* dest, long n) {
  for (long i = 0; i < n; i++)
    dest[i] = source[i] + 1;
}

void Fork_join() {
  std::cout << "Fork-join examples" << std::endl;

  std::cout << "Example: Fork join" << std::endl;
  std::cout << "-----------------------" << std::endl;
  {
    long b1 = 0;
    long b2 = 0;
    long j  = 0;
    
    par::fork2([&] {
      // first branch
      b1 = 1;
    }, [&] {
      // second branch
      b2 = 2;
    });
    // join point
    j = b1 + b2;
    
    std::cout << "b1 = " << b1 << "; b2 = " << b2 << "; ";
    std::cout << "j = " << j << ";" << std::endl;
  }
  std::cout << "-----------------------" << std::endl;
  
  std::cout << "Example: Writes and the join statement" << std::endl;
  std::cout << "-----------------------" << std::endl;
  {
    long b1 = 0;
    long b2 = 0;
    
    par::fork2([&] {
      b1 = 1;
    }, [&] {
      b2 = 2;
    });
    std::cout << "b1 = " << b1 << "; b2 = " << b2 << std::endl;
  }
  std::cout << "-----------------------" << std::endl;
  
  std::cout << "Example: use of map_incr" << std::endl;
  std::cout << "-----------------------" << std::endl;
  {
    const long n = 4;
    long xs[n] = { 1, 2, 3, 4 };
    long ys[n];
    map_incr(xs, ys, n);
    for (long i = 0; i < n; i++)
      std::cout << ys[i] << " ";
    std::cout << std::endl;
  }
  std::cout << "-----------------------" << std::endl;
}

sparray fill_seq(long n, value_type x) {
  sparray tmp = sparray(n);
  for (long i = 0; i < n; i++)
    tmp[i] = x;
  return tmp;
}

void bar() {
  sparray xs = fill_seq(4, 1234);
  std::cout << "xs = " << xs << std::endl;
}

auto is_even = [] (value_type x) {
  return (x%2) == 0;
};

sparray extract_evens(const sparray& xs) {
  return filter([&] (value_type x) { return is_even(x); }, xs);
}

void Simple_parallel_arrays () {
  
  std::cout << "Example: Simple use of arrays" << std::endl;
  std::cout << "-----------------------" << std::endl;
  {
    sparray xs = { 1, 2, 3 };
    std::cout << "xs[1] = " << xs[1] << std::endl;
    std::cout << "xs.size() = " << xs.size() << std::endl;
    xs[2] = 5;
    std::cout << "xs[2] = " << xs[2] << std::endl;
  }
  std::cout << "-----------------------" << std::endl;
  
  std::cout << "Example: Allocation and deallocation" << std::endl;
  std::cout << "-----------------------" << std::endl;
  {
    sparray zero_length = sparray();
    sparray another_zero_length = sparray(0);
    sparray yet_another_zero_length;
    sparray length_five = sparray(5);    // contents uninitialized
    std::cout << "|zero_length| = " << zero_length.size() << std::endl;
    std::cout << "|another_zero_length| = " << another_zero_length.size() << std::endl;
    std::cout << "|yet_another_zero_length| = " << yet_another_zero_length.size() << std::endl;
    std::cout << "|length_five| = " << length_five.size() << std::endl;
  }
  std::cout << "-----------------------" << std::endl;
  
  std::cout << "Example: Create and initialize an array (sequentially)" << std::endl;
  std::cout << "-----------------------" << std::endl;
  {
    bar();
  }
  std::cout << "-----------------------" << std::endl;
  
  std::cout << "Example: Ownership-passing semantics" << std::endl;
  std::cout << "-----------------------" << std::endl;
  {
    sparray xs = fill_seq(4, 1234);
    sparray ys = fill_seq(3, 333);
    ys = std::move(xs);
    std::cout << "xs = " << xs << std::endl;
    std::cout << "ys = " << ys << std::endl;
  }
  std::cout << "-----------------------" << std::endl;
}

void Data_parallelism() {
  std::cout << "Data-parallelism examples" << std::endl;
  
  std::cout << "Example: Sequences of even numbers" << std::endl;
  std::cout << "-----------------------" << std::endl;
  {
    sparray evens = tabulate([&] (long i) { return 2*i; }, 5);
    std::cout << "evens = " << evens << std::endl;

  }
  std::cout << "-----------------------" << std::endl;
  
  std::cout << "Example: Solution to homework exercise: summing elements of array" << std::endl;
  std::cout << "-----------------------" << std::endl;
  {
    auto plus_fct = [&] (value_type x, value_type y) {
      return x+y;
    };

    sparray xs = { 1, 2, 3 };
    std::cout << "sum_xs = " << reduce(plus_fct, 0, xs) << std::endl;
  }
  std::cout << "-----------------------" << std::endl;
  
  std::cout << "Example: Solution to homework exercise: taking max of elements of array" << std::endl;
  std::cout << "-----------------------" << std::endl;
  {
    auto max_fct = [&] (value_type x, value_type y) {
      return std::max(x, y);
    };
    
    sparray xs = { -3, 1, 634, 2, 3 };
    std::cout << "reduce(max_fct, xs[0], xs) = " << reduce(max_fct, xs[0], xs) << std::endl;
  }
  std::cout << "-----------------------" << std::endl;
  
  std::cout << "Example: Inclusive scan" << std::endl;
  std::cout << "-----------------------" << std::endl;
  {
    std::cout << scan_incl(plus_fct, 0, sparray({ 2, 1, 8, 3 })) << std::endl;
  }
  std::cout << "-----------------------" << std::endl;
  
  std::cout << "Example: Exclusive scan" << std::endl;
  std::cout << "-----------------------" << std::endl;
  {
    scan_excl_result res = scan_excl(plus_fct, 0, { 2, 1, 8, 3 });
    std::cout << "partials = " << res.partials << std::endl;
    std::cout << "total = " << res.total << std::endl;
  }
  std::cout << "-----------------------" << std::endl;
  
  std::cout << "Example: Creating an array of all 3s" << std::endl;
  std::cout << "-----------------------" << std::endl;
  {
    sparray threes = fill(3, 5);
    std::cout << "threes = " << threes << std::endl;
  }
  std::cout << "-----------------------" << std::endl;

  std::cout << "Example: Copying an array" << std::endl;
  std::cout << "-----------------------" << std::endl;
  {
    sparray xs = { 3, 2, 1 };
    sparray ys = copy(xs);
    std::cout << "xs = " << xs << std::endl;
    std::cout << "ys = " << ys << std::endl;

  }
  std::cout << "-----------------------" << std::endl;
  
  std::cout << "Example: Slicing an array" << std::endl;
  std::cout << "-----------------------" << std::endl;
  {
    sparray xs = { 1, 2, 3, 4, 5 };
    std::cout << "slice(xs, 1, 3) = " << slice(xs, 1, 3) << std::endl;
    std::cout << "slice(xs, 0, 4) = " << slice(xs, 0, 4) << std::endl;
  }
  std::cout << "-----------------------" << std::endl;
  
  std::cout << "Example: Concatenating two arrays" << std::endl;
  std::cout << "-----------------------" << std::endl;
  {
    sparray xs = { 1, 2, 3 };
    sparray ys = { 4, 5 };
    std::cout << "concat(xs, ys) = " << concat(xs, ys) << std::endl;
  }
  std::cout << "-----------------------" << std::endl;
  
  std::cout << "Example: Inclusive and exclusive prefix sums" << std::endl;
  std::cout << "-----------------------" << std::endl;
  {
    sparray xs = { 2, 1, 8, 3 };
    sparray incl = prefix_sums_incl(xs);
    scan_excl_result excl = prefix_sums_excl(xs);
    std::cout << "incl = " << incl << std::endl;
    std::cout << "excl.partials = " << excl.partials << "; excl.total = " << excl.total << std::endl;

  }
  std::cout << "-----------------------" << std::endl;
  
  std::cout << "Example: Extracting even numbers" << std::endl;
  std::cout << "-----------------------" << std::endl;
  {
    sparray xs = { 3, 5, 8, 12, 2, 13, 0 };
    std::cout << "extract_evens(xs) = " << extract_evens(xs) << std::endl;
  }
  std::cout << "-----------------------" << std::endl;
  
  std::cout << "Example: The allocation problem" << std::endl;
  std::cout << "-----------------------" << std::endl;
  {
    /*
    sparray flags = { true, false, false, true, false, true, true };
    sparray xs    = {   34,    13,     5,    1,    41,   11,   10 };
    std::cout << "pack(flags, xs) = " << pack(flags, xs) << std::endl;
     */
    std::cout << "pack example currently broken" << std::endl;
  }
  std::cout << "-----------------------" << std::endl;
}

void Sorting() {
  std::cout << "Sorting examples" << std::endl;
  /*
  std::cout << "Example: The allocation problem" << std::endl;
  std::cout << "-----------------------" << std::endl;
  {
    sparray xs = {
      // first range: [0, 4)
      5, 10, 13, 14,
      // second range: [4, 9)
      1, 8,  10, 100, 101 };
    
    sparray tmp = sparray(xs.size());
    merge_par(xs, tmp, (long)0, (long)4, (long)9);
    
    std::cout << "xs = " << xs << std::endl;
  }
  std::cout << "-----------------------" << std::endl;
  */

}

loop_controller_type concurrent_counter_contr ("parallel for");

void concurrent_counter(long n) {
  long counter = 0;

  auto incr = [&] () {
    counter = counter + 1;
  };
    
  par::parallel_for(concurrent_counter_contr, 0l, n, [&] (long i) {
      incr(); 
  });

  std::cout << "Concurrent-counter: " << "n = " << n << " result = " << counter << std::endl;
}


loop_controller_type concurrent_counter_atomic_contr ("parallel for");

void concurrent_counter_atomic(long n) {
  std::atomic<long> counter;
  counter.store(0);

  auto incr = [&] () {
    while (true) { 
      long current = counter.load();
      if (counter.compare_exchange_strong (current,current+1)) {
        break;
      }
    }
  };

  par::parallel_for(concurrent_counter_atomic_contr, 0l, n, [&] (long i) {
      incr(); 
  });

  std::cout << "Concurrent-counter-atomic: " << "n = " << n << " result = " << counter << std::endl;
}


loop_controller_type concurrent_counter_atomic_contr_aba ("parallel for");

void concurrent_counter_atomic_aba(long n) {
  std::atomic<long> counter;
  counter.store(0);

  auto incr_decr = [&] (long i) {
    if (i-2*(i/2) == 0) {
      while (true) { 
        long current = counter.load();
  	if (counter.compare_exchange_strong (current,current+1)) {
          break;
	}
      }}
    else {
      while (true) { 
        long current = counter.load();
  	if (counter.compare_exchange_strong (current,current-1)) {
          break;
	}
      }}

  };

  par::parallel_for(concurrent_counter_atomic_contr_aba, 0l, n, [&] (long i) {
      incr_decr(i); 
  });

  std::cout << "Concurrent-counter-atomic-aba: " << "n = " << n << " result = " << counter << std::endl;
}


void Graph_processing() {
  std::cout << "Graph-processing examples" << std::endl;
  
  std::cout << "Example: Graph creation" << std::endl;
  std::cout << "-----------------------" << std::endl;
  {
    adjlist graph = { mk_edge(0, 1), mk_edge(0, 3), mk_edge(5, 1), mk_edge(3, 0) };
    std::cout << graph << std::endl;
  }
  std::cout << "-----------------------" << std::endl;
  
  std::cout << "Example: Adjacency-list interface" << std::endl;
  std::cout << "-----------------------" << std::endl;
  {
    adjlist graph = { mk_edge(0, 1), mk_edge(0, 3), mk_edge(5, 1), mk_edge(3, 0),
                      mk_edge(3, 5), mk_edge(3, 2), mk_edge(5, 3) };
    std::cout << "nb_vertices = " << graph.get_nb_vertices() << std::endl;
    std::cout << "nb_edges = " << graph.get_nb_edges() << std::endl;
    std::cout << "out_edges of vertex 3:" << std::endl;
    neighbor_list out_edges_of_3 = graph.get_out_edges_of(3);
    for (long i = 0; i < graph.get_out_degree_of(3); i++)
      std::cout << " " << out_edges_of_3[i];
    std::cout << std::endl;
  }
  std::cout << "-----------------------" << std::endl;
  
  std::cout << "Example: Accessing the contents of atomic memory cells" << std::endl;
  std::cout << "-----------------------" << std::endl;
  {
    const long n = 3;
    std::atomic<bool> visited[n];
    long v = 2;
    visited[v].store(false);
    std::cout << visited[v].load() << std::endl;
    visited[v].store(true);
    std::cout << visited[v].load() << std::endl;
  }
  std::cout << "-----------------------" << std::endl;

  std::cout << "Example: Compare and exchange" << std::endl;
  std::cout << "-----------------------" << std::endl;
  {
    const long n = 3;
    std::atomic<bool> visited[n];
    long v = 2;
    visited[v].store(false);
    bool orig = false;
    bool was_successful = visited[v].compare_exchange_strong(orig, true);
    std::cout << "was_successful = " << was_successful << "; visited[v] = " << visited[v].load() << std::endl;
    bool orig2 = false;
    bool was_successful2 = visited[v].compare_exchange_strong(orig2, true);
    std::cout << "was_successful2 = " << was_successful2 << "; visited[v] = " << visited[v].load() << std::endl;
  }
  std::cout << "-----------------------" << std::endl;
  
  std::cout << "Example: Parallel BFS" << std::endl;
  std::cout << "-----------------------" << std::endl;
  {
    adjlist graph = { mk_edge(0, 1), mk_edge(0, 3), mk_edge(5, 1), mk_edge(3, 0),
                      mk_edge(3, 5), mk_edge(3, 2), mk_edge(5, 3),
                      mk_edge(4, 6), mk_edge(6, 2) };
    std::cout << graph << std::endl;
    sparray reachable_from_0 = bfs(graph, 0);
    std::cout << "reachable from 0: " << reachable_from_0 << std::endl;
    sparray reachable_from_4 = bfs(graph, 4);
    std::cout << "reachable from 4: " << reachable_from_4 << std::endl;
  }
  std::cout << "-----------------------" << std::endl;

  std::cout << "Example: Edge map" << std::endl;
  std::cout << "-----------------------" << std::endl;
  {
    adjlist graph = { mk_edge(0, 1), mk_edge(0, 3), mk_edge(5, 1), mk_edge(3, 0),
                      mk_edge(3, 5), mk_edge(3, 2), mk_edge(5, 3),
                      mk_edge(4, 6), mk_edge(6, 2) };
    const long n = graph.get_nb_vertices();
    std::atomic<bool> visited[n];
    for (long i = 0; i < n; i++)
      visited[i] = false;
    visited[0].store(true);
    visited[1].store(true);
    visited[3].store(true);
    sparray in_frontier = { 3 };
    sparray out_frontier = edge_map(graph, visited, in_frontier);
    std::cout << out_frontier << std::endl;
    sparray out_frontier2 = edge_map(graph, visited, in_frontier);
    std::cout << out_frontier2 << std::endl;
  }
  std::cout << "-----------------------" << std::endl;
}

/*---------------------------------------------------------------------*/
/* Examples from exercises */

void merge_exercise_example() {
  std::cout << "Merge exercise example" << std::endl;
/*
  std::cout << "-----------------------" << std::endl;
  sparray xs = { 2, 4, 6, 8 };
  sparray ys = { 5, 5, 13, 21, 23 };
  long lo_xs = 1;
  long hi_xs = 3;
  long lo_ys = 0;
  long hi_ys = 4;
  long lo_tmp = 0;
  long hi_tmp = (hi_xs-lo_xs) + (hi_ys+lo_ys);
  sparray tmp = sparray(hi_tmp);
  exercises::merge_par(xs, ys, tmp, lo_xs, hi_xs, lo_ys, hi_ys, lo_tmp);
  std::cout << "xs =" << slice(xs, lo_xs, hi_xs) << std::endl;
  std::cout << "ys =" << slice(ys, lo_ys, hi_ys) << std::endl;
  std::cout << "tmp = " << tmp << std::endl;
  std::cout << "-----------------------" << std::endl;
*/
  /*
   When your merge exercise is complete, the output should be
   the following:
   
   xs ={ 4, 6 }
   ys ={ 5, 5, 13, 21 }
   tmp = { 4, 5, 5, 6, 13, 21 }
   */
}

/*---------------------------------------------------------------------*/
/* PASL Driver */

int main(int argc, char** argv) {
  
  auto init = [&] {
   
  };
  auto run = [&] (bool) {
    // Defaultly, all of the following function calls are performed
    // on execution of this program.
    // To run just one, say, "sorting", pass to this program the option:
    // -example sorting.
    pasl::util::cmdline::argmap_dispatch c;
    c.add("fork-join", [&] { Fork_join(); });
    c.add("simple-parallel-arrays", [&] { Simple_parallel_arrays(); });
    c.add("data-parallelism", [&] { Data_parallelism(); });
    c.add("sorting", [&] { Sorting(); });
    // counters
    long n = pasl::util::cmdline::parse_or_default_long ("n",1000000);
    c.add("concurrent_counter", [&] { concurrent_counter(n); });
    c.add("concurrent_counter_atomic", [&] { concurrent_counter_atomic(n); });
    c.add("concurrent_counter_atomic_aba", [&] { concurrent_counter_atomic_aba(n); });
    //c.add("graph-processing", [&] { Graph_processing(); });
    c.add("merge-exercise", [&] { merge_exercise_example(); });
    // Add an option for your example code here:
    // c.add("your-example", [&] { your_function(); });
    pasl::util::cmdline::dispatch_by_argmap_with_default_all(c, "example");
  };
  auto output = [&] {
  };
  auto destroy = [&] {
  };
  pasl::sched::launch(argc, argv, init, run, output, destroy);
}

/***********************************************************************/

