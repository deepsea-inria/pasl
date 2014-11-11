#include <fstream>

#include "graphgenerators.hpp"
#include "graphconversions.hpp"
#include "ls_bag.hpp"
#include "frontierseg.hpp"
#include "dijkstra.hpp"
#include "benchmark.hpp"

namespace pasl {
namespace graph {
  
/***********************************************************************/
  
using namespace data;
  
int nb_tests = 1000;

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
/* Dijkstra */

template <class Adjlist_seq>
void check_dijkstra() {
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
    
    auto trusted_dijkstra = [&] (const adjlist_type& graph, vtxid_type source) -> vtxid_type* {
        vtxid_type nb_vertices = graph.get_nb_vertices();
        if (nb_vertices == 0)
            return NULL;
        return dijkstra_dummy(graph, source);
    };

  c.add("dijkstra_dummy_test", [&] {
      auto by_vertexid_frontier = [&] (const adjlist_type& graph, vtxid_type source) -> vtxid_type* {
          vtxid_type nb_vertices = graph.get_nb_vertices();
          if (nb_vertices == 0)
              return NULL;
          return dijkstra_dummy(graph, source);
      };
    using prop_by_vertexid_frontier =
    prop_search_same<adjlist_type, typeof(trusted_dijkstra), typeof(by_vertexid_frontier),
    typeof(get_visited_seq), typeof(get_visited_seq), int>;
    prop_by_vertexid_frontier (trusted_dijkstra, by_vertexid_frontier,
                               get_visited_seq, get_visited_seq).check(nb_tests);
  });

  util::cmdline::dispatch_by_argmap_with_default_all(c, "algo");
}
  
/*---------------------------------------------------------------------*/
  
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
    c.add("dijkstra",         [] { pasl::graph::check_dijkstra<adjlist_seq_type>(); });
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

