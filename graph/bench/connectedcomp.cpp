/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file search.cpp
 *
 */

#include <vector>

#include "graphfileshared.hpp"
#include "benchmark.hpp"
#include "dfs.hpp"

namespace pasl {
namespace graph {

/***********************************************************************/
  
template <class Vertex_id>
class component_info {
public:
  Vertex_id source_vertex;
  Vertex_id nb_vertices_in_component;
  component_info(Vertex_id s, Vertex_id n)
  : source_vertex(s), nb_vertices_in_component(n) {}
};
  
template <class Adjlist_seq>
void connected_components_by_serial_dfs(
    const adjlist<Adjlist_seq>& graph,
    std::vector<component_info<typename adjlist<Adjlist_seq>::vtxid_type>>& components) {
  using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
  using component_info_type = component_info<vtxid_type>;
  vtxid_type nb_vertices = graph.get_nb_vertices();
  int* visited = data::mynew_array<int>(nb_vertices);
  fill_array_seq(visited, nb_vertices, 0);
  for (vtxid_type v = 0; v < nb_vertices; v++) {
    if (visited[v])
      continue;
    long nb_vertices_visited = 0;
    dfs_by_vertexid_array<Adjlist_seq,false,true>(graph, v, nullptr, &nb_vertices_visited, visited);
    components.push_back(component_info_type(v, vtxid_type(nb_vertices_visited)));
  }
}

/*---------------------------------------------------------------------*/

template <class Adjlist>
void connectedcomp() {
  using vtxid_type = typename Adjlist::vtxid_type;
  long nb_components_to_report;
  using component_info_type = component_info<vtxid_type>;
  std::vector<component_info_type> components;
  Adjlist graph;
  auto init = [&] {
    nb_components_to_report = util::cmdline::parse_or_default_uint64("nb_components_to_report", 10);
  };
  auto run = [&] (bool sequential) {
    load_graph_from_file(graph);
    connected_components_by_serial_dfs(graph, components);
  };
  auto output = [&] {
    vtxid_type nb_components = (vtxid_type)components.size();
    std::cout << "nb_vertices\t" << graph.get_nb_vertices() << std::endl;
    std::cout << "nb_edges\t" << graph.nb_edges << std::endl;
    std::cout << "nb_components\t" << nb_components << std::endl;

    auto compare_fct = [] (component_info_type c1, component_info_type c2) {
      return c1.nb_vertices_in_component < c2.nb_vertices_in_component;
    };
    std::sort(components.begin(), components.end(), compare_fct);

    vtxid_type nb = std::min(vtxid_type(nb_components_to_report), nb_components);
    for (vtxid_type i = 0; i < nb; i++) {
      component_info_type c = components[nb_components - i - 1];
      std::cout << "component\t(" << c.source_vertex << ", " << c.nb_vertices_in_component << ")" << std::endl;
    }
    
    std::cout << std::endl;
  };
  auto destroy = [&] { };
  sched::launch(init, run, output, destroy);
  return;
}

} // end namespace
} // end namespace

/***********************************************************************/

using namespace pasl;

int main(int argc, char ** argv) {
  util::cmdline::set(argc, argv);

  using vtxid_type32 = int;
  using adjlist_seq_type32 = graph::flat_adjlist_seq<vtxid_type32>;
  using adjlist_type32 = graph::adjlist<adjlist_seq_type32>;

  using vtxid_type64 = long;
  using adjlist_seq_type64 = graph::flat_adjlist_seq<vtxid_type64>;
  using adjlist_type64 = graph::adjlist<adjlist_seq_type64>;

  int nb_bits = util::cmdline::parse_or_default_int("bits", 32);

  if (nb_bits == 32 )
    graph::connectedcomp<adjlist_type32>();
  else if (nb_bits == 64)
    graph::connectedcomp<adjlist_type64>();
  else
    util::atomic::die("bits must be either 32 or 64");

  return 0;
}
