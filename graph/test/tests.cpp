/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file tests.cpp
 *
 */

#include <fstream>

#include "graphgenerators.hpp"
#include "graphconversions.hpp"
#include "ls_bag.hpp"
#include "frontierseg.hpp"
#include "bfs.hpp"
#include "dfs.hpp"
#include "benchmark.hpp"

namespace pasl {
namespace graph {
  
/***********************************************************************/
  
using namespace data;
  
int nb_tests = 1000;

template <class Item, class Size, class Op>
Item reduce_std_atomic(std::atomic<Item>* array, Size sz, const Op& op) {
  auto get_from_array = [&] (Size i) {
    return array[i].load();
  };
  return pbbs::sequence::reduce<int>(Size(0), sz, op, get_from_array);
}

  /*
auto add2_atomic = [] (const std::atomic<int>& a, const std::atomic<int>& b) {
  return a.load() + b.load();
};
   */

template <class Size, class Values_equal,
class Array1, class Array2,
class Get_value1, class Get_value2>
bool same_arrays(Size sz,
                 Array1 array1, Array2 array2,
                 const Get_value1& get_value1, const Get_value2& get_value2,
                 const Values_equal& equals) {
  for (Size i = 0; i < sz; i++)
    if (! equals(get_value1(array1, i), get_value2(array2, i)))
      return false;
  return true;
}
  
/*---------------------------------------------------------------------*/
/* Graph format conversion */

template <class Edgelist, class Adjlist>
class prop_graph_format_conversion_identity : public quickcheck::Property<Edgelist> {
public:
  
  using edgelist_type = Edgelist;
  using adjlist_type = Adjlist;
  
  bool holdsFor(const edgelist_type& graph) {
    adjlist_type adj;
    adjlist_from_edgelist(graph, adj);
    edgelist_type edg;
    edgelist_from_adjlist(adj, edg);
    return edg == graph;
  }
  
};

void check_conversion() {
  
  using vtxid_type = int;
  
  using edge_type = edge<vtxid_type>;
  using edgelist_bag_type = data::stl::vector_seq<edge_type>;
  using edgelist_type = edgelist<edgelist_bag_type>;
  
  using adjlist_seq_type = flat_adjlist_seq<vtxid_type>;
  using adjlist_type = adjlist<adjlist_seq_type>;
  
  std::cout << "conversion" << std::endl;
  prop_graph_format_conversion_identity<edgelist_type, adjlist_type> prop;
  prop.check(nb_tests);
}
  
/*---------------------------------------------------------------------*/
/* Graph search */
  
template <class Item>
std::ostream& operator<<(std::ostream& out, const std::pair<long, Item*>& seq) {
  long n = seq.first;
  Item* array = seq.second;
  for (int i = 0; i < n; i++) {
    out << array[i];
    if (i + 1 < n)
      out << ",\t";
  }
  return out;
}

template <class Adjlist, class Search_trusted, class Search_to_test,
class Get_value1, class Get_value2,
class Res>
class prop_search_same : public quickcheck::Property<Adjlist> {
public:
  
  using adjlist_type = Adjlist;
  using vtxid_type = typename adjlist_type::vtxid_type;
  
  Search_trusted search_trusted;
  Search_to_test search_to_test;
  Get_value1 get_value1;
  Get_value2 get_value2;
  
  prop_search_same(const Search_trusted& search_trusted,
                   const Search_to_test& search_to_test,
                   const Get_value1& get_value1,
                   const Get_value2& get_value2)
  : search_trusted(search_trusted), search_to_test(search_to_test),
  get_value1(get_value1), get_value2(get_value2) { }
  
  int nb = 0;
  bool holdsFor(const adjlist_type& graph) {
    nb++;
    if (nb == 18)
      std::cout << "" ;
    vtxid_type nb_vertices = graph.get_nb_vertices();
    vtxid_type source;
    quickcheck::generate(std::max(vtxid_type(0), nb_vertices-1), source);
    source = std::abs(source);
    auto res_trusted = search_trusted(graph, source);
    auto res_to_test = search_to_test(graph, source);
    if (res_trusted == NULL || res_to_test == NULL)
      return true;
    bool success = same_arrays(nb_vertices, res_trusted, res_to_test,
                               get_value1, get_value2,
                               [] (Res x, Res y) {return x == y;});
    if (! success) {
      std::cout << "source:    " << source << std::endl;
      std::cout << "trusted:   " << std::make_pair(nb_vertices,res_trusted) << std::endl;
      std::cout << "untrusted: " << std::make_pair(nb_vertices,res_to_test) << std::endl;
    }
    return success;
  }
  
};
  
/*---------------------------------------------------------------------*/
/* BFS */

template <class Adjlist_seq>
void check_bfs() {
  using adjlist_type = adjlist<Adjlist_seq>;
  using vtxid_type = typename adjlist_type::vtxid_type;
  using chunkedbag_type = pcontainer::bag<vtxid_type>;
  using ls_bag_type = data::Bag<vtxid_type>;
  using adjlist_alias_type = typename adjlist_type::alias_type;

  using frontiersegbag_type = pasl::graph::frontiersegbag<adjlist_alias_type>;
  
  util::cmdline::argmap_dispatch c;
  
  auto get_visited_seq = [] (vtxid_type* dists, vtxid_type i) {
    return (dists[i] != graph_constants<vtxid_type>::unknown_vtxid) ? 1 : 0;
  };
  
  auto get_visited_par = [] (std::atomic<vtxid_type>* dists, vtxid_type i) {
    vtxid_type dist = dists[i].load();
    return (dist != graph_constants<vtxid_type>::unknown_vtxid) ? 1 : 0;
  };
  
  auto trusted_bfs = [&] (const adjlist_type& graph, vtxid_type source) -> vtxid_type* {
    vtxid_type nb_vertices = graph.get_nb_vertices();
    if (nb_vertices == 0)
      return NULL;
    return bfs_by_array(graph, source);
  };
  
  c.add("dynamic_array", [&] {
    auto by_dynamic_array = [&] (const adjlist_type& graph, vtxid_type source) -> vtxid_type* {
      vtxid_type nb_vertices = graph.get_nb_vertices();
      if (nb_vertices == 0)
        return NULL;
      return bfs_by_dynamic_array<Adjlist_seq, data::stl::deque_seq<vtxid_type>>(graph, source);
    };
    using prop_by_dynamic_array =
    prop_search_same<adjlist_type, typeof(trusted_bfs), typeof(by_dynamic_array),
    typeof(get_visited_seq), typeof(get_visited_seq), vtxid_type>;
    prop_by_dynamic_array (trusted_bfs, by_dynamic_array, get_visited_seq, get_visited_seq).check(nb_tests);
  });
  c.add("dual_arrays", [&] {
    auto by_dual_arrays = [&] (const adjlist_type& graph, vtxid_type source) -> vtxid_type* {
      vtxid_type nb_vertices = graph.get_nb_vertices();
      if (nb_vertices == 0)
        return NULL;
      return bfs_by_dual_arrays(graph, source);
    };
    using prop_by_dual_arrays =
    prop_search_same<adjlist_type, typeof(trusted_bfs), typeof(by_dual_arrays),
    typeof(get_visited_seq), typeof(get_visited_seq), vtxid_type>;
    prop_by_dual_arrays (trusted_bfs, by_dual_arrays, get_visited_seq, get_visited_seq).check(nb_tests);
  });
  c.add("dual_frontiers_and_pushpop", [&] {
    auto by_dual_frontiers_and_pushpop = [&] (const adjlist_type& graph, vtxid_type source) -> vtxid_type* {
      vtxid_type nb_vertices = graph.get_nb_vertices();
      if (nb_vertices == 0)
        return NULL;
      return bfs_by_dual_frontiers_and_pushpop<Adjlist_seq, data::stl::vector_seq<vtxid_type>>(graph, source);
    };
    using prop_by_dual_frontiers_and_pushpop =
    prop_search_same<adjlist_type, typeof(trusted_bfs), typeof(by_dual_frontiers_and_pushpop),
    typeof(get_visited_seq), typeof(get_visited_seq), vtxid_type>;
    prop_by_dual_frontiers_and_pushpop (trusted_bfs, by_dual_frontiers_and_pushpop,
                                        get_visited_seq, get_visited_seq).check(nb_tests);
  });
  c.add("dual_frontiers_and_foreach", [&] {
    auto by_dual_frontiers_and_foreach = [&] (const adjlist_type& graph, vtxid_type source) -> vtxid_type* {
      vtxid_type nb_vertices = graph.get_nb_vertices();
      if (nb_vertices == 0)
        return NULL;
      return bfs_by_dual_frontiers_and_foreach<Adjlist_seq, data::stl::vector_seq<vtxid_type>>(graph, source);
    };
    using prop_by_dual_frontiers_and_foreach =
    prop_search_same<adjlist_type, typeof(trusted_bfs), typeof(by_dual_frontiers_and_foreach),
    typeof(get_visited_seq), typeof(get_visited_seq), vtxid_type>;
    prop_by_dual_frontiers_and_foreach (trusted_bfs, by_dual_frontiers_and_foreach,
                                        get_visited_seq, get_visited_seq).check(nb_tests);
  });
  c.add("frontier_segment", [&] {
    auto by_frontier_segment = [&] (const adjlist_type& graph, vtxid_type source) -> vtxid_type* {
      vtxid_type nb_vertices = graph.get_nb_vertices();
      if (nb_vertices == 0)
        return NULL;
      return bfs_by_frontier_segment<adjlist_type, frontiersegbag_type>(graph, source);
    };
    using prop_by_frontier_segment =
    prop_search_same<adjlist_type, typeof(trusted_bfs), typeof(by_frontier_segment),
    typeof(get_visited_seq), typeof(get_visited_seq), vtxid_type>;
    prop_by_frontier_segment (trusted_bfs, by_frontier_segment,
                              get_visited_seq, get_visited_seq).check(nb_tests);
  });
  c.add("pbbs_ndbfs", [&] {
    auto by_pbbs_ndbfs = [&] (const adjlist_type& graph, vtxid_type source) -> std::atomic<vtxid_type>* {
      vtxid_type nb_vertices = graph.get_nb_vertices();
      if (nb_vertices == 0)
        return NULL;
      return pbbs_pbfs<false, adjlist_type>(graph, source);
    };
    using prop_pbbs_ndbfs =
    prop_search_same<adjlist_type, typeof(trusted_bfs), typeof(by_pbbs_ndbfs),
    typeof(get_visited_seq), typeof(get_visited_par), vtxid_type>;
    prop_pbbs_ndbfs (trusted_bfs, by_pbbs_ndbfs, get_visited_seq, get_visited_par).check(nb_tests);
  });
  c.add("ls_pbfs_with_our_bag", [&] {
    auto by_pbfs = [&] (const adjlist_type& graph, vtxid_type source) -> std::atomic<vtxid_type>* {
      vtxid_type nb_vertices = graph.get_nb_vertices();
      if (nb_vertices == 0)
        return NULL;
      return ls_pbfs<false>::main<Adjlist_seq, chunkedbag_type>(graph, source);
    };
    using prop_pbfs =
    prop_search_same<adjlist_type, typeof(trusted_bfs), typeof(by_pbfs),
    typeof(get_visited_seq), typeof(get_visited_par), vtxid_type>;
    prop_pbfs (trusted_bfs, by_pbfs, get_visited_seq, get_visited_par).check(nb_tests);
  });
  c.add("ls_pbfs", [&] {
    auto by_pbfs = [&] (const adjlist_type& graph, vtxid_type source) -> std::atomic<vtxid_type>* {
      vtxid_type nb_vertices = graph.get_nb_vertices();
      if (nb_vertices == 0)
        return NULL;
      return ls_pbfs<false>::main<Adjlist_seq, ls_bag_type>(graph, source);
    };
    using prop_pbfs =
    prop_search_same<adjlist_type, typeof(trusted_bfs), typeof(by_pbfs),
    typeof(get_visited_seq), typeof(get_visited_par), vtxid_type>;
    prop_pbfs (trusted_bfs, by_pbfs, get_visited_seq, get_visited_par).check(nb_tests);
  });
  c.add("our_pbfs", [&] {
    auto by_fpbfs = [&] (const adjlist_type& graph, vtxid_type source) -> std::atomic<vtxid_type>* {
      vtxid_type nb_vertices = graph.get_nb_vertices();
      if (nb_vertices == 0)
        return NULL;
      return our_bfs<false>::main<adjlist_type, frontiersegbag_type>(graph, source);
    };
    using prop_fpbfs =
    prop_search_same<adjlist_type, typeof(trusted_bfs), typeof(by_fpbfs),
    typeof(get_visited_seq), typeof(get_visited_par), vtxid_type>;
    prop_fpbfs (trusted_bfs, by_fpbfs, get_visited_seq, get_visited_par).check(nb_tests);
  });
  c.add("our_lazy_pbfs", [&] {
    auto by_fpbfs = [&] (const adjlist_type& graph, vtxid_type source) -> std::atomic<vtxid_type>* {
      vtxid_type nb_vertices = graph.get_nb_vertices();
      if (nb_vertices == 0)
        return NULL;
      return our_lazy_bfs<false>::main<adjlist_type, frontiersegbag_type>(graph, source);
    };
    using prop_fpbfs =
    prop_search_same<adjlist_type, typeof(trusted_bfs), typeof(by_fpbfs),
    typeof(get_visited_seq), typeof(get_visited_par), vtxid_type>;
    prop_fpbfs (trusted_bfs, by_fpbfs, get_visited_seq, get_visited_par).check(nb_tests);
  });
  util::cmdline::dispatch_by_argmap_with_default_all(c, "algo");
}
  
/*---------------------------------------------------------------------*/
/* DFS */

template <class Adjlist_seq>
void check_dfs() {
  using adjlist_type = adjlist<Adjlist_seq>;
  using vtxid_type = typename adjlist_type::vtxid_type;
  using adjlist_alias_type = typename adjlist_type::alias_type;
  
  using frontiersegstack_type = pasl::graph::frontiersegstack<adjlist_alias_type>;
  
  util::cmdline::argmap_dispatch c;
  
  auto get_visited_seq = [] (int* visited, vtxid_type i) {
    return visited[i];
  };
  auto get_visited_par = [] (std::atomic<int>* visited, vtxid_type i) {
    return visited[i].load();
  };
  
  auto trusted_dfs = [&] (const adjlist_type& graph, vtxid_type source) -> int* {
    vtxid_type nb_vertices = graph.get_nb_vertices();
    if (nb_vertices == 0)
      return 0;
    return dfs_by_vertexid_array(graph, source);
  };
  
  c.add("vertexid_frontier", [&] {
    auto by_vertexid_frontier = [&] (const adjlist_type& graph, vtxid_type source) -> int* {
      vtxid_type nb_vertices = graph.get_nb_vertices();
      if (nb_vertices == 0)
        return 0;
      return dfs_by_vertexid_frontier<Adjlist_seq, data::stl::vector_seq<vtxid_type>>(graph, source);
    };
    using prop_by_vertexid_frontier =
    prop_search_same<adjlist_type, typeof(trusted_dfs), typeof(by_vertexid_frontier),
    typeof(get_visited_seq), typeof(get_visited_seq), int>;
    prop_by_vertexid_frontier (trusted_dfs, by_vertexid_frontier,
                               get_visited_seq, get_visited_seq).check(nb_tests);
  });
  c.add("frontier_segment", [&] {
    auto by_frontier_segment = [&] (const adjlist_type& graph, vtxid_type source) -> int* {
      vtxid_type nb_vertices = graph.get_nb_vertices();
      if (nb_vertices == 0)
        return 0;
      return dfs_by_frontier_segment<adjlist_type, frontiersegstack_type>(graph, source);
    };
    using prop_by_frontier_segment =
    prop_search_same<adjlist_type, typeof(trusted_dfs), typeof(by_frontier_segment),
    typeof(get_visited_seq), typeof(get_visited_seq), int>;
    prop_by_frontier_segment (trusted_dfs, by_frontier_segment,
                              get_visited_seq, get_visited_seq).check(nb_tests);
  });
  c.add("pseudodfs", [&] {
    auto by_pseudodfs = [&] (const adjlist_type& graph, vtxid_type source) -> std::atomic<int>* {
      vtxid_type nb_vertices = graph.get_nb_vertices();
      if (nb_vertices == 0)
        return 0;
      return our_pseudodfs<adjlist_type, frontiersegstack_type>(graph, source);
    };
    using prop_by_pseudodfs =
    prop_search_same<adjlist_type, typeof(trusted_dfs), typeof(by_pseudodfs),
    typeof(get_visited_seq), typeof(get_visited_par), int>;
    prop_by_pseudodfs (trusted_dfs, by_pseudodfs,
                       get_visited_seq, get_visited_par).check(nb_tests);
  });
  c.add("cong_pseudodfs", [&] {
    auto by_cong_pseudodfs = [&] (const adjlist_type& graph, vtxid_type source) -> std::atomic<int>* {
      vtxid_type nb_vertices = graph.get_nb_vertices();
      if (nb_vertices == 0)
        return 0;
      return cong_pseudodfs<Adjlist_seq>(graph, source);
    };
    using prop_by_cong_pseudodfs =
    prop_search_same<adjlist_type, typeof(trusted_dfs), typeof(by_cong_pseudodfs),
    typeof(get_visited_seq), typeof(get_visited_par), int>;
    prop_by_cong_pseudodfs (trusted_dfs, by_cong_pseudodfs,
                            get_visited_seq, get_visited_par).check(nb_tests);
  });
  util::cmdline::dispatch_by_argmap_with_default_all(c, "algo");
}
  
/*---------------------------------------------------------------------*/
/* IO */

template <class Adjlist>
class prop_file_io_preserves_adjlist : public quickcheck::Property<Adjlist> {
public:
  
  using adjlist_type = Adjlist;
  using vtxid_type = typename adjlist_type::vtxid_type;
  
  bool holdsFor(const adjlist_type& graph) {
    std::string fname = "foobar.bin";
    write_adjlist_to_file(fname, graph);
    adjlist_type graph2;
    read_adjlist_from_file(fname, graph2);
    bool success = graph == graph2;
    graph.check();
    graph2.check();
    return success;
  }
  
};

template <class Vertex_id>
void check_io() {
  using vtxid_type = Vertex_id;
  using adjlist_seq_type = flat_adjlist_seq<vtxid_type>;
  using adjlist_type = adjlist<adjlist_seq_type>;
  
  std::cout << "file io" << std::endl;
  prop_file_io_preserves_adjlist<adjlist_type> prop;
  prop.check(nb_tests);
}

int cong_pdfs_cutoff = 16;
int our_pseudodfs_cutoff = 16;
int ls_pbfs_cutoff = 256;
int ls_pbfs_loop_cutoff = 256;
int our_bfs_cutoff = 8;
int our_lazy_bfs_cutoff = 8;

  
bool should_disable_random_permutation_of_vertices;
  
} // end namespace
} // end namespace

/*---------------------------------------------------------------------*/

int main(int argc, char ** argv) {
  using vtxid_type = long;
  using adjlist_seq_type = pasl::graph::flat_adjlist_seq<vtxid_type>;
  
  auto init = [&] {
    pasl::graph::should_disable_random_permutation_of_vertices = pasl::util::cmdline::parse_or_default_bool("should_disable_random_permutation_of_vertices", false, false);
    pasl::graph::nb_tests = pasl::util::cmdline::parse_or_default_int("nb_tests", 1000);
    pasl::graph::ls_pbfs_cutoff = pasl::util::cmdline::parse_or_default_int("ls_pbfs_cutoff", 64);
  };
  auto run = [&] (bool sequential) {
    pasl::util::cmdline::argmap_dispatch c;
    c.add("dfs",         [] { pasl::graph::check_dfs<adjlist_seq_type>(); });
    c.add("bfs",         [] { pasl::graph::check_bfs<adjlist_seq_type>(); });
    c.add("io",          [] { pasl::graph::check_io<vtxid_type>(); });
    c.add("conversion",  [] { pasl::graph::check_conversion(); });
    pasl::util::cmdline::dispatch_by_argmap_with_default_all(c, "test");
  };
  auto output = [&] {
    std::cout << "All tests complete" << std::endl;
  };
  auto destroy = [&] {
    ;
  };
  pasl::sched::launch(argc, argv, init, run, output, destroy);

  return 0;
}

/***********************************************************************/

