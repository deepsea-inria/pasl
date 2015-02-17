/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file graphconversions.hpp
 * \brief Conversions between graph formats
 */

#include "adjlist.hpp"
#include "edgelist.hpp"
#include "native.hpp"
#include "blockradixsort.hpp"

#ifndef _PASL_GRAPH_CONVERSIONS_H_
#define _PASL_GRAPH_CONVERSIONS_H_

/***********************************************************************/

namespace pasl {
namespace graph {
  
template <class Edge_bag, class Adjlist_seq>
void adjlist_from_edgelist(const edgelist<Edge_bag>& edg, adjlist<Adjlist_seq>& adj) {
  util::atomic::die("todo");
}



// serial algorithm
template <class Edge_bag, class Vertex_id>
void adjlist_from_edgelist(const edgelist<Edge_bag>& edg, adjlist<flat_adjlist_seq<Vertex_id>>& adj) {
  using vtxid_type = typename Edge_bag::value_type::vtxid_type;
  using adjlist_type = adjlist<flat_adjlist_seq<Vertex_id>>;
  using edge_type = typename Edge_bag::value_type;
  if (sizeof(vtxid_type) > sizeof(typename adjlist_type::vtxid_type))
    util::atomic::die("conversion failed due to incompatible types");
  edg.check();
  vtxid_type nb_vertices = edg.nb_vertices;
  vtxid_type nb_offsets = nb_vertices + 1;
  edgeid_type nb_edges = edg.get_nb_edges();
  edgeid_type contents_sz = nb_offsets + nb_edges * 2;
    char* contents = (char*)data::mynew_array<vtxid_type>(contents_sz);
    char* contents_in = (char*)data::mynew_array<vtxid_type>(contents_sz);
    
  adj.adjlists.init(contents, contents_in, nb_vertices, nb_edges);
    vtxid_type* offsets = adj.adjlists.offsets;
    vtxid_type* offsets_in = adj.adjlists.offsets_in;
    
    vtxid_type* edges = adj.adjlists.edges;
    vtxid_type* edges_in = adj.adjlists.edges_in;
    
    data::array_seq<vtxid_type> degrees;
    data::array_seq<vtxid_type> degrees_in;
    
    degrees.alloc(nb_vertices);
    degrees_in.alloc(nb_vertices);
    
    for (vtxid_type i = 0; i < nb_vertices; i++) {
        degrees[i] = 0;
        degrees_in[i] = 0;
    }
  for (edgeid_type i = 0; i < edg.edges.size(); i++) {
    edge_type e = edg.edges[i];
    degrees[e.src]++;
      degrees_in[e.dst]++;
  }
  offsets[0] = 0;
  for (vtxid_type i = 1; i < nb_offsets; i++)
    offsets[i] = offsets[i - 1] + 2 * degrees[i - 1];
    
    offsets_in[0] = 0;
    for (vtxid_type i = 1; i < nb_offsets; i++)
        offsets_in[i] = offsets_in[i - 1] + 2 * degrees_in[i - 1];
    for (vtxid_type i = 0; i < nb_vertices; i++) {
        degrees[i] = 0;
        degrees_in[i] = 0;
        
    }
    
  for (edgeid_type i = 0; i < edg.edges.size(); i++) {
    edge_type e = edg.edges[i];
    vtxid_type cnt = degrees[e.src];
      edges[offsets[e.src] + 2 * cnt] = e.dst;
      edges[offsets[e.src] + 2 * cnt + 1] = e.w;
      
    degrees[e.src]++;

      vtxid_type cnt_in = degrees_in[e.dst];
      edges_in[offsets_in[e.dst] + 2 * cnt_in] = e.src;
      edges_in[offsets_in[e.dst] + 2 * cnt_in + 1] = e.w;
      degrees_in[e.dst]++;
  }
    
  adj.nb_edges = nb_edges;
  adj.check();
}


template <class Adjlist_seq, class Edge_bag>
void edgelist_from_adjlist(const adjlist<Adjlist_seq>& adj, edgelist<Edge_bag>& edg) {
  using edge_type = typename Edge_bag::value_type;
  using vtxid_type = typename edge_type::vtxid_type;
  assert(sizeof(vtxid_type) == sizeof(typename adjlist<Adjlist_seq>::vtxid_type));
  adj.check();
  edg.edges.alloc(adj.nb_edges);
  vtxid_type k = 0;
  for (vtxid_type i = 0; i < adj.get_nb_vertices(); i++)
    for (vtxid_type j = 0; j < adj.adjlists[i].get_out_degree(); j++)
      edg.edges[k++] = edge_type(i, adj.adjlists[i].get_out_neighbor(j));
  edg.nb_vertices = adj.get_nb_vertices();
  edg.check();
}

template <class Vertex_id>
flat_adjlist_alias<Vertex_id> get_alias_of_adjlist(const flat_adjlist<Vertex_id>& graph) {
  flat_adjlist_alias<Vertex_id> alias;
  alias.adjlists = graph.adjlists.get_alias();
  alias.nb_edges = graph.nb_edges;
  return alias;
}
  
/*---------------------------------------------------------------------*/
/* Random permutation of vertex ids of an edgelist */

template <class Vertex_id>
Vertex_id* create_random_vertex_permutation_table(Vertex_id nb_vertices) {
  Vertex_id* t = data::mynew_array<Vertex_id>(nb_vertices);
  sched::native::parallel_for(Vertex_id(0), nb_vertices, [&] (Vertex_id i) {
    t[i] = i;
  });
  /*
  for (Vertex_id i = 0; i < nb_vertices; i++) { // later: parallelize when we have parallel rng
    Vertex_id j = Vertex_id(rand()) % (i+1);
    std::swap(t[i], t[j]);
  }
   */
  for (Vertex_id i = 1; i < nb_vertices; i++) { // later: parallelize when we have parallel rng
    Vertex_id j = 1 + Vertex_id(rand()) % i;
    std::swap(t[i], t[j]);
  }
  return t;
}

template <class Edge_bag>
void randomly_permute_vertex_ids(edgelist<Edge_bag>& edg) {
  using vtxid_type = typename Edge_bag::value_type::vtxid_type;
  using edge_type = typename Edge_bag::value_type;
  vtxid_type nb_vertices = edg.nb_vertices;
  vtxid_type* perm = create_random_vertex_permutation_table(nb_vertices);
  auto perm_edge = [&] (edge_type e) {
    return edge_type(perm[e.src], perm[e.dst]);
  };
  sched::native::parallel_for(edgeid_type(0), edg.edges.size(), [&] (edgeid_type i) {
    edg.edges[i] = perm_edge(edg.edges[i]);
  });
  data::myfree(perm);
}
  
} // end namespace
} // end namespace

/***********************************************************************/

#endif /*! _PASL_GRAPH_CONVERSIONS_H_ */