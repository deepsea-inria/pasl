#include <fstream>

#include "graphgenerators.hpp"
#include "graphconversions.hpp"
#include "ls_bag.hpp"
#include "frontierseg.hpp"
#include "bellman_ford.hpp"
#include "benchmark.hpp"

namespace pasl {
namespace graph {
  
/***********************************************************************/
  
using namespace data;
  
int nb_tests = 2;

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
/* Bellman ford */
    


template <class Adjlist_seq>
void check_bellman_ford() {
    using adjlist_type = adjlist<Adjlist_seq>;
    using vtxid_type = typename adjlist_type::vtxid_type;
    using chunkedbag_type = pcontainer::bag<vtxid_type>;
    using ls_bag_type = data::Bag<vtxid_type>;
    using adjlist_alias_type = typename adjlist_type::alias_type;
    
    using frontiersegbag_type = pasl::graph::frontiersegbag<adjlist_alias_type>;
    
    util::cmdline::argmap_dispatch c;
    
    auto get_visited_seq = [] (int* visited, vtxid_type i) {
        return visited[i];
    };

    
    auto trusted_bellman_ford = [&] (const adjlist_type& graph, vtxid_type source) -> int* {
        vtxid_type nb_vertices = graph.get_nb_vertices();
        if (nb_vertices == 0)
            return 0;
        int* res =  bellman_ford_seq(graph, source);
//        std::cout << "Source : " << source << std::endl;
//        std::cout << graph << std::endl;
//
//        for (int i = 0; i < nb_vertices; ++i) {
//            std::cout << res[i] << " ";
//        }
        std::cout << std::endl;
        return res;
    };
    
    c.add("bellman_ford_seq", [&] {
        uint64_t start_time = util::microtime::now();
        
        auto par_test = [&] (const adjlist_type& graph, vtxid_type source) -> int* {
            vtxid_type nb_vertices = graph.get_nb_vertices();
            if (nb_vertices == 0)
                return 0;
            return bellman_ford_seq(graph, source);
        };
        using prop_by_vertexid_frontier =
        prop_search_same<adjlist_type, typeof(trusted_bellman_ford), typeof(par_test),
        typeof(get_visited_seq), typeof(get_visited_seq), int>;
        prop_by_vertexid_frontier (trusted_bellman_ford, par_test,
                                   get_visited_seq, get_visited_seq).check(nb_tests);
        double exec_time = util::microtime::seconds_since(start_time);
        printf ("exectime BF Seq  %.3lf\n", exec_time);
    });

    c.add("bellman_ford_par1", [&] {
        uint64_t start_time = util::microtime::now();
        
        auto par_test = [&] (const adjlist_type& graph, vtxid_type source) -> int* {
            vtxid_type nb_vertices = graph.get_nb_vertices();
            if (nb_vertices == 0)
                return 0;
            return bellman_ford_par1(graph, source);
        };
        using prop_by_vertexid_frontier =
        prop_search_same<adjlist_type, typeof(trusted_bellman_ford), typeof(par_test),
        typeof(get_visited_seq), typeof(get_visited_seq), int>;
        prop_by_vertexid_frontier (trusted_bellman_ford, par_test,
                                   get_visited_seq, get_visited_seq).check(nb_tests);
        double exec_time = util::microtime::seconds_since(start_time);
        printf ("exectime BF Par1 %.3lf\n", exec_time);
    });
    
    c.add("bellman_ford_par2", [&] {
        uint64_t start_time = util::microtime::now();
        
        auto par_test = [&] (const adjlist_type& graph, vtxid_type source) -> int* {
            vtxid_type nb_vertices = graph.get_nb_vertices();
            if (nb_vertices == 0)
                return 0;
            return bellman_ford_par2(graph, source);
        };
        using prop_by_vertexid_frontier =
        prop_search_same<adjlist_type, typeof(trusted_bellman_ford), typeof(par_test),
        typeof(get_visited_seq), typeof(get_visited_seq), int>;
        prop_by_vertexid_frontier (trusted_bellman_ford, par_test,
                                   get_visited_seq, get_visited_seq).check(nb_tests);
        
        double exec_time = util::microtime::seconds_since(start_time);
        printf ("exectime BF Par2 %.3lf\n", exec_time);
    });
    
    c.add("bellman_ford_par3", [&] {
        uint64_t start_time = util::microtime::now();
        
        auto par_test = [&] (const adjlist_type& graph, vtxid_type source) -> int* {
            vtxid_type nb_vertices = graph.get_nb_vertices();
            if (nb_vertices == 0)
                return 0;
            return bellman_ford_par3(graph, source);
        };
        using prop_by_vertexid_frontier =
        prop_search_same<adjlist_type, typeof(trusted_bellman_ford), typeof(par_test),
        typeof(get_visited_seq), typeof(get_visited_seq), int>;
        prop_by_vertexid_frontier (trusted_bellman_ford, par_test,
                                   get_visited_seq, get_visited_seq).check(nb_tests);
        
        double exec_time = util::microtime::seconds_since(start_time);
        printf ("exectime BF Par3 %.3lf\n", exec_time);
    });

  util::cmdline::dispatch_by_argmap_with_default_all(c, "algo");
}

} // end namespace
} // end namespace

/*---------------------------------------------------------------------*/

int main(int argc, char ** argv) {
    using vtxid_type = long;
    using adjlist_seq_type = pasl::graph::flat_adjlist_seq<vtxid_type>;
  
    auto init = [&] {
        pasl::graph::nb_tests = pasl::util::cmdline::parse_or_default_int("nb_tests", pasl::graph::nb_tests);
    };
    auto run = [&] (bool sequential) {
        pasl::util::cmdline::argmap_dispatch c;
        c.add("bellman_ford",         [] { pasl::graph::check_bellman_ford<adjlist_seq_type>(); });
        pasl::util::cmdline::dispatch_by_argmap_with_default_all(c, "test");
    };
    auto output = [&] {
        std::cout << "All tests complete" << std::endl;
    };
    auto destroy = [&] {};
    pasl::sched::launch(argc, argv, init, run, output, destroy);

    return 0;
}

/***********************************************************************/

