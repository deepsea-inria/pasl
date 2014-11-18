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
    int* dijkstra_dummy_seq(const adjlist<Adjlist_seq>& graph,
                 typename adjlist<Adjlist_seq>::vtxid_type source) {
        using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
        int inf_dist = shortest_path_constants<int>::inf_dist;
        vtxid_type nb_vertices = graph.get_nb_vertices();
        int* dists = data::mynew_array<int>(nb_vertices);
        bool* used = data::mynew_array<bool>(nb_vertices);
        
        fill_array_seq(dists, nb_vertices, inf_dist);
        fill_array_seq(used, nb_vertices, false);
        
        dists[source] = 0;
        LOG_BASIC(ALGO_PHASE);
        for (size_t cnt = 0; cnt < nb_vertices; cnt++) {
            vtxid_type vertex;
            int min_dist = inf_dist;
            for (vtxid_type v = 0; v < nb_vertices; v++) {
                if (!used[v] && min_dist > dists[v]) {
                    min_dist = dists[v];
                    vertex = v;
                }
            }
            used[vertex] = true;
            vtxid_type degree = graph.adjlists[vertex].get_out_degree();
            vtxid_type* neighbors = graph.adjlists[vertex].get_out_neighbors();
            for (vtxid_type edge = 0; edge < degree; edge++) {
                vtxid_type other = neighbors[edge];
                dists[other] = std::min(dists[other], dists[vertex] + 1);
            }
        }
        free(used);
        return dists;
    }
    
    /*---------------------------------------------------------------------*/
    /*---------------------------------------------------------------------*/
    /*---------------------------------------------------------------------*/
    /* Dijkstra's algorithm; dummy version */
    
    // Just simple version without any optimizations
    template <class Adjlist_seq>
    int* dijkstra_dummy1(const adjlist<Adjlist_seq>& graph,
                        typename adjlist<Adjlist_seq>::vtxid_type source) {
        using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
        int inf_dist = shortest_path_constants<int>::inf_dist;
        vtxid_type nb_vertices = graph.get_nb_vertices();
        int* dists = data::mynew_array<int>(nb_vertices);
        bool* used = data::mynew_array<bool>(nb_vertices);
        
        fill_array_seq(dists, nb_vertices, inf_dist);
        fill_array_seq(used, nb_vertices, false);
        
        dists[source] = 0;
        LOG_BASIC(ALGO_PHASE);
        for (size_t cnt = 0; cnt < nb_vertices; cnt++) {
            vtxid_type vertex;
            int min_dist = inf_dist;
            for (vtxid_type v = 0; v < nb_vertices; v++) {
                if (!used[v] && min_dist > dists[v]) {
                    min_dist = dists[v];
                    vertex = v;
                }
            }
            used[vertex] = true;
            vtxid_type degree = graph.adjlists[vertex].get_out_degree();
            vtxid_type* neighbors = graph.adjlists[vertex].get_out_neighbors();
            for (vtxid_type edge = 0; edge < degree; edge++) {
                vtxid_type other = neighbors[edge];
                dists[other] = std::min(dists[other], dists[vertex] + 1);
            }
        }
        free(used);
        return dists;
    }

} // end namespace
} // end namespace

/***********************************************************************/

#endif /*! _PASL_GRAPH_BFS_H_ */
