
#include "edgelist.hpp"
#include "adjlist.hpp"
#include "pcontainer.hpp"

#ifndef _PASL_GRAPH_BELLMAN_FORD_H_
#define _PASL_GRAPH_BELLMAN_FORD_H_

#define PUSH_ZERO_ARITY_VERTICES 0

/***********************************************************************/

namespace pasl {
namespace graph {

    /*---------------------------------------------------------------------*/
    /*---------------------------------------------------------------------*/
    /*---------------------------------------------------------------------*/
    /* Bellman-Ford; serial */
    /*---------------------------------------------------------------------*/
    template <class Adjlist_seq>
    int* bellman_ford_seq(const adjlist<Adjlist_seq>& graph,
                            typename adjlist<Adjlist_seq>::vtxid_type source) {
        using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
        int inf_dist = shortest_path_constants<int>::inf_dist;
        vtxid_type nb_vertices = graph.get_nb_vertices();
        int* dists = data::mynew_array<int>(nb_vertices);
        
        fill_array_seq(dists, nb_vertices, inf_dist);

        dists[source] = 0;
        
        LOG_BASIC(ALGO_PHASE);
        for (size_t step = 0; step < nb_vertices; step++) {
            bool changed = false;
            for (size_t i = 0; i < nb_vertices; i++) {
                vtxid_type degree = graph.adjlists[i].get_out_degree();
                vtxid_type* neighbors = graph.adjlists[i].get_out_neighbors();
                for (vtxid_type edge = 0; edge < degree; edge++) {
                    vtxid_type other = neighbors[edge];
                    if (dists[other] > dists[i] + 1) {
                        changed = true;
                        dists[other] = dists[i] + 1;
                    }
                }
            }
            if (!changed) break;
        }
        return dists;
    }
    
    /*---------------------------------------------------------------------*/
    /*---------------------------------------------------------------------*/
    /*---------------------------------------------------------------------*/
    /* Bellman-Ford; parallel */
    /*---------------------------------------------------------------------*/
    template <class Adjlist_seq>
    int* bellman_ford_par(const adjlist<Adjlist_seq>& graph,
                          typename adjlist<Adjlist_seq>::vtxid_type source) {
        using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
        int inf_dist = shortest_path_constants<int>::inf_dist;
        vtxid_type nb_vertices = graph.get_nb_vertices();
        int* dists = data::mynew_array<int>(nb_vertices);
        
        fill_array_seq(dists, nb_vertices, inf_dist);
        
        dists[source] = 0;
        
        LOG_BASIC(ALGO_PHASE);
        for (size_t step = 0; step < nb_vertices; step++) {
            bool changed = false;
            for (size_t i = 0; i < nb_vertices; i++) {
                vtxid_type degree = graph.adjlists[i].get_out_degree();
                vtxid_type* neighbors = graph.adjlists[i].get_out_neighbors();
                for (vtxid_type edge = 0; edge < degree; edge++) {
                    vtxid_type other = neighbors[edge];
                    if (dists[other] > dists[i] + 1) {
                        changed = true;
                        dists[other] = dists[i] + 1;
                    }
                }
            }
            if (!changed) break;
        }
        
        return dists;
    }



} // end namespace
} // end namespace

/***********************************************************************/

#endif /*! _PASL_GRAPH_BFS_H_ */
