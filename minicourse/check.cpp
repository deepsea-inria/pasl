/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file check.cpp
 * \brief Unit testing driver
 *
 */

#include <math.h>
#include <iostream>
#include <fstream>

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

/*---------------------------------------------------------------------*/
/* Quickcheck library initialization */

namespace quickcheck {
  
  void generate(size_t target_nb_edges, edgelist& edges);
  
  std::ostream& operator<<(std::ostream& out, const edgelist& edges) {
    return output_directed_dot(out, edges);
  }
  
}

#include "quickcheck.hh" // needs to appear at end of include list

/*---------------------------------------------------------------------*/

long nb_tests;
std::string outfile;

template <class Property>
void checkit(std::string msg) {
  if (outfile == "") {
    quickcheck::check<Property>(msg.c_str(), nb_tests);
  } else {
    std::ofstream f(outfile);
    if (f.is_open()) {
      bool isVerbose = false;
      size_t max = 5 * nb_tests;
      quickcheck::check<Property>(msg.c_str(), nb_tests, max, isVerbose, f);
    } else {
      std::cerr << "Failed to open output file " << outfile << std::endl;
    }
  }

}

bool same_sparray(const sparray& xs, const sparray& ys) {
  if (xs.size() != ys.size())
    return false;
  for (long i = 0; i < xs.size(); i++)
    if (xs[i] != ys[i])
      return false;
  return true;
}

sparray sparray_of_vector(const std::vector<value_type>& vec) {
  return tabulate([&] (long i) { return vec[i]; }, vec.size());
}

/*---------------------------------------------------------------------*/
/* Unit tests for MCSS */

using namespace quickcheck;

template <class Trusted_mcss_fct, class Untrusted_mcss_fct>
class mcss_correct : public quickcheck::Property<std::vector<value_type>> {
public:
  
  Trusted_mcss_fct trusted;
  Untrusted_mcss_fct untrusted;
  
  bool holdsFor(const std::vector<value_type>& vec) {
    sparray xs = sparray_of_vector(vec);
    value_type x = trusted(xs);
    value_type y = untrusted(xs);
    return x == y;
  }
  
};

void check_mcss() {
  class trusted_fct {
  public:
    value_type operator()(const sparray& xs) {
      return mcss_seq(xs);
    }
  };
  class untrusted_fct {
  public:
    value_type operator()(const sparray& xs) {
      return mcss_par(xs);
    }
  };
  using property_type = mcss_correct<trusted_fct, untrusted_fct>;
  checkit<property_type>("mcss is correct");
}

/*---------------------------------------------------------------------*/
/* Unit tests for sorting algorithms */

template <class Trusted_sort_fct, class Untrusted_sort_fct>
class sort_correct : public quickcheck::Property<std::vector<value_type>> {
public:
  
  Trusted_sort_fct trusted_sort;
  Untrusted_sort_fct untrusted_sort;
  
  bool holdsFor(const std::vector<value_type>& vec) {
    sparray xs = sparray_of_vector(vec);
    return same_sparray(trusted_sort(xs), untrusted_sort(xs));
  }
  
};

/*---------------------------------------------------------------------*/
/* Unit tests for graph algorithms */

edgelist gen_random_edgelist(unsigned long dim, unsigned long degree, unsigned long nb_rows) {
  edgelist edges;
  long nb_nonzeros = degree * nb_rows;
  edges.resize(nb_nonzeros);
  for (long k = 0; k < nb_nonzeros; k++) {
    unsigned long i = k / degree;
    unsigned long j;
    if (dim==0) {
      unsigned long h = k;
      do {
        j = ((h = hash_unsigned(h)) % nb_rows);
      } while (j == i);
    } else {
      unsigned long pow = dim+2;
      unsigned long h = k;
      do {
        while ((((h = hash_unsigned(h)) % 1000003) < 500001)) pow += dim;
        j = (i + ((h = hash_unsigned(h)) % (((unsigned long) 1) << pow))) % nb_rows;
      } while (j == i);
    }
    edges[k].first = i;
    edges[k].second = j;
  }
  return edges;
}

edgelist gen_random_edgelist(long target_nb_edges) {
  long dim = 10;
  long degree = 8;
  long nb_rows = std::min(degree, target_nb_edges);
  return gen_random_edgelist(dim, degree, nb_rows);
}

adjlist gen_random_adjlist(long target_nb_edges) {
  return adjlist(gen_random_edgelist(target_nb_edges));
}

edgelist gen_balanced_tree_edgelist(long branching_factor, long height) {
  std::vector<vtxid_type> prev;
  std::vector<vtxid_type> next;
  edgelist edges;
  prev.push_back(vtxid_type(0));
  vtxid_type fresh = 1;
  for (vtxid_type level = 0; level < height; level++) {
    for (auto it = prev.begin(); it != prev.end(); it++) {
      vtxid_type v = *it;
      for (vtxid_type n = 0; n < branching_factor; n++) {
        vtxid_type child = fresh;
        fresh++;
        next.push_back(child);
        edges.push_back(mk_edge(v, child));
      }
    }
    prev.clear();
    prev.swap(next);
  }
  return edges;
}

edgelist gen_balanced_tree_edgelist(long target_nb_edges) {
  long branching_factor = 2;
  long height = log2_up(target_nb_edges) - 1;
  return gen_balanced_tree_edgelist(branching_factor, height);
}

adjlist gen_balanced_tree_adjlist(long target_nb_edges) {
  return adjlist(gen_balanced_tree_edgelist(target_nb_edges));
}

edgelist gen_cube_grid_edgelist(long nb_on_side, long) {
  edgelist edges;
  long dn = nb_on_side;
  long nn = dn*dn*dn;
  long nb_edges = 3*nn;
  edges.resize(nb_edges);
  auto loc3d = [&](vtxid_type x, vtxid_type y, vtxid_type z) {
    return ((x + dn) % dn)*dn*dn + ((y + dn) % dn)*dn + (z + dn) % dn;
  };
  for (long i = 0; i < dn; i++) {
    for (vtxid_type j = 0; j < dn; j++) {
      for (vtxid_type k = 0; k < dn; k++) {
        vtxid_type l = loc3d(i,j,k);
        edges[3*l] =   mk_edge(l,loc3d(i+1,j,k));
        edges[3*l+1] = mk_edge(l,loc3d(i,j+1,k));
        edges[3*l+2] = mk_edge(l,loc3d(i,j,k+1));
      }
    }
  }
  return edges;
}

edgelist gen_cube_grid_edgelist(long target_nb_edges) {
  long nb_on_side = long(pow(double(target_nb_edges / 3.0), 1.0/3.0));
  return gen_cube_grid_edgelist(nb_on_side, 0l);
}

adjlist gen_cube_grid_adjlist(long target_nb_edges) {
  return adjlist(gen_cube_grid_edgelist(target_nb_edges));
}

namespace quickcheck {
  void generate(size_t target_nb_edges, edgelist& edges) {
    std::vector<std::function<void ()>> gens;
    gens.push_back([&] {
      edges = gen_random_edgelist(target_nb_edges);
    });
    gens.push_back([&] {
      edges = gen_cube_grid_edgelist(target_nb_edges);
    });
    gens.push_back([&] {
      edges = gen_balanced_tree_edgelist(target_nb_edges);
    });
    long i = random() % gens.size();
    gens[i]();
  }
}

template <class Trusted_bfs_fct, class Untrusted_bfs_fct>
class bfs_correct : public quickcheck::Property<edgelist> {
public:
  
  Trusted_bfs_fct trusted_bfs;
  Untrusted_bfs_fct untrusted_bfs;
  
  bool holdsFor(const edgelist& edges) {
    adjlist graph = adjlist(edges);
    return same_sparray(trusted_bfs(graph, 0), untrusted_bfs(graph, 0));
  }
  
};

void check_graph() {
  pasl::util::cmdline::argmap_dispatch c;
  class trusted_fct {
  public:
    sparray operator()(const adjlist& graph, vtxid_type source) {
      return bfs_seq(graph, source);
    }
  };
  c.add("bfs", [&] {
    class untrusted_fct {
    public:
      sparray operator()(const adjlist& graph, vtxid_type source) {
        return bfs(graph, source);
      }
    };
    using property_type = bfs_correct<trusted_fct, untrusted_fct>;
    checkit<property_type>("BFS is correct");
  });
  c.find_by_arg("algo")();
}

void check_graph_ex() {
  pasl::util::cmdline::argmap_dispatch c;
  class trusted_fct {
  public:
    sparray operator()(const adjlist& graph, vtxid_type source) {
      return bfs_seq(graph, source);
    }
  };
  c.add("bfs", [&] {
    class untrusted_fct {
    public:
      sparray operator()(const adjlist& graph, vtxid_type source) {
        return exercises::bfs(graph, source);
      }
    };
    using property_type = bfs_correct<trusted_fct, untrusted_fct>;
    checkit<property_type>("BFS is correct");
  });
  c.find_by_arg("algo")();
}

/*---------------------------------------------------------------------*/
/* Unit tests for student exercises */

value_type* ptr_on_first_item(sparray& xs) {
  if (xs.size() == 0)
    return nullptr;
  return &xs[0];
}

class map_incr_ex_correct : public quickcheck::Property<std::vector<value_type>> {
public:
  
  bool holdsFor(const std::vector<value_type>& vec) {
    sparray xs = sparray_of_vector(vec);
    sparray dest = sparray(xs.size());
    exercises::map_incr(ptr_on_first_item(xs), ptr_on_first_item(dest), xs.size());
    return same_sparray(dest, map(plus1_fct, xs));
  }
  
};

void check_map_incr_ex() {
  checkit<map_incr_ex_correct>("solution to map_incr exercise is correct");
}

class max_ex_correct : public quickcheck::Property<std::vector<value_type>> {
public:
  
  bool holdsFor(const std::vector<value_type>& vec) {
    sparray xs = sparray_of_vector(vec);
    return exercises::max(ptr_on_first_item(xs), xs.size()) == max(xs);
  }
  
};

void check_max_ex() {
  checkit<max_ex_correct>("solution to max exercise is correct");
}

class plus_ex_correct : public quickcheck::Property<std::vector<value_type>> {
public:
  
  bool holdsFor(const std::vector<value_type>& vec) {
    sparray xs = sparray_of_vector(vec);
    
    return exercises::plus(ptr_on_first_item(xs), xs.size()) == sum(xs);
  }
  
};

void check_plus_ex() {
  checkit<plus_ex_correct>("solution to plus exercise is correct");
}

class reduce_ex_correct : public quickcheck::Property<std::vector<value_type>> {
public:
  
  bool holdsFor(const std::vector<value_type>& vec) {
    sparray xs = sparray_of_vector(vec);
    return exercises::reduce(plus_fct, 0l, ptr_on_first_item(xs), xs.size()) == sum(xs);
  }
  
};

void check_reduce_ex() {
  checkit<reduce_ex_correct>("solution to plus exercise is correct");
}

class duplicate_correct : public quickcheck::Property<std::vector<value_type>> {
public:
  
  bool holdsFor(const std::vector<value_type>& vec) {
    sparray xs = sparray_of_vector(vec);
    return same_sparray(exercises::duplicate(xs), duplicate(xs));
  }
  
};

void check_duplicate() {
  checkit<duplicate_correct>("solution to duplicate is correct");
}

class ktimes_correct : public quickcheck::Property<std::vector<value_type>> {
public:
  
  bool holdsFor(const std::vector<value_type>& vec) {
    sparray xs = sparray_of_vector(vec);
    long k = (rand() % 5) + 1;
    return same_sparray(exercises::ktimes(xs, k), ktimes(xs, k));
  }
  
};

void check_ktimes() {
  checkit<ktimes_correct>("solution to ktimes is correct");
}

class filter_ex_correct : public quickcheck::Property<std::vector<value_type>> {
public:
  
  bool holdsFor(const std::vector<value_type>& vec) {
    sparray xs = sparray_of_vector(vec);
    return same_sparray(exercises::filter(is_even_fct, xs), filter(is_even_fct, xs));
  }
  
};

void check_filter_ex() {
  checkit<filter_ex_correct>("solution to filter is correct");
}

/*---------------------------------------------------------------------*/
/* PASL Driver */

void check() {
  nb_tests = pasl::util::cmdline::parse_or_default_long("nb_tests", 500);
  outfile = pasl::util::cmdline::parse_or_default_string("outfile", "");
  pasl::util::cmdline::argmap_dispatch c;
  c.add("mcss", std::bind(check_mcss));
  class trusted_sort_fct {
  public:
    sparray operator()(const sparray& xs) {
      return seqsort(xs);
    }
  };
  c.add("mergesort", [&] {
    class untrusted_fct {
    public:
      sparray operator()(const sparray& xs) {
        return mergesort(xs);
      }
    };
    using property_type = sort_correct<trusted_sort_fct, untrusted_fct>;
    checkit<property_type>("mergesort is correct");
  });
  c.add("mergesort_ex", [&] {
    class untrusted_fct {
    public:
      sparray operator()(const sparray& xs) {
        return exercises::mergesort(xs);
      }
    };
    using property_type = sort_correct<trusted_sort_fct, untrusted_fct>;
    checkit<property_type>("mergesort is correct");
  });
  c.add("cilksort", [&] {
    class untrusted_fct {
    public:
      sparray operator()(const sparray& xs) {
        return cilksort(xs);
      }
    };
    using property_type = sort_correct<trusted_sort_fct, untrusted_fct>;
    checkit<property_type>("mergesort is correct");
  });
  c.add("quicksort", [&] {
    class untrusted_fct {
    public:
      sparray operator()(const sparray& xs) {
        return quicksort(xs);
      }
    };
    using property_type = sort_correct<trusted_sort_fct, untrusted_fct>;
    checkit<property_type>("quicksort is correct");
  });
  c.add("graph", std::bind(check_graph));
  c.add("map_incr_ex", std::bind(check_map_incr_ex));
  c.add("max_ex", std::bind(check_max_ex));
  c.add("plus_ex", std::bind(check_plus_ex));
  c.add("reduce_ex", std::bind(check_reduce_ex));
  c.add("duplicate_ex", std::bind(check_duplicate));
  c.add("ktimes_ex", std::bind(check_ktimes));
  c.add("filter_ex", std::bind(check_filter_ex));
  c.add("graph_ex", std::bind(check_graph_ex));
  c.find_by_arg("check")();
}

int main(int argc, char** argv) {
  
  auto init = [&] {
    
  };
  auto run = [&] (bool) {
    check();
  };
  auto output = [&] {
  };
  auto destroy = [&] {
  };
  pasl::sched::launch(argc, argv, init, run, output, destroy);
}

/***********************************************************************/
