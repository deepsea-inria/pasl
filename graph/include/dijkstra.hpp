#include "edgelist.hpp"
#include "adjlist.hpp"
#include "pcontainer.hpp"

#ifndef _PASL_GRAPH_DIJKSTRA_H_
#define _PASL_GRAPH_DIJKSTRA_H_


#define PUSH_ZERO_ARITY_VERTICES 0



/***********************************************************************/

namespace pasl {
namespace graph {

/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/
/* Dijkstra's algorithm; dummy version */

// Just simple version without any optimizations
template <class Adjlist_seq>
typename adjlist<Adjlist_seq>::vtxid_type*
dijkstra_dummy(const adjlist<Adjlist_seq>& graph,
             typename adjlist<Adjlist_seq>::vtxid_type source) {
  using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
  vtxid_type unknown = graph_constants<vtxid_type>::unknown_vtxid;
  vtxid_type nb_vertices = graph.get_nb_vertices();
  vtxid_type* dists = data::mynew_array<vtxid_type>(nb_vertices);
  fill_array_seq(dists, nb_vertices, unknown);
  LOG_BASIC(ALGO_PHASE);
  vtxid_type* queue = data::mynew_array<vtxid_type>(2 * nb_vertices);
  const vtxid_type next_dist_token = vtxid_type(-2); // todo: change to maxint-1
  vtxid_type head = 0;
  vtxid_type tail = 0;
  vtxid_type dist = 0;
  dists[source] = 0;
  queue[tail++] = source;
  queue[tail++] = next_dist_token;
  while (tail - head > 1) {
    vtxid_type vertex = queue[head++];
    if (vertex == next_dist_token) {
      dist++;
      queue[tail++] = next_dist_token;
      continue;
    }
    vtxid_type degree = graph.adjlists[vertex].get_out_degree();
    vtxid_type* neighbors = graph.adjlists[vertex].get_out_neighbors();
    for (vtxid_type edge = 0; edge < degree; edge++) {
      vtxid_type other = neighbors[edge];
      if (dists[other] != unknown)
        continue;
      dists[other] = dist+1;
      if (PUSH_ZERO_ARITY_VERTICES || graph.adjlists[other].get_out_degree() > 0)
        queue[tail++] = other;
    }
  }
  free(queue);
  return dists;
}

} // end namespace
} // end namespace

/***********************************************************************/

#endif /*! _PASL_GRAPH_BFS_H_ */
