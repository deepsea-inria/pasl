#include "base_algo.hpp"
#include "edgelist.hpp"
#include "adjlist.hpp"
#include "pcontainer.hpp"
#include <thread>
#include <unordered_map>

#ifndef _PASL_GRAPH_FLOYD_H_
#define _PASL_GRAPH_FLOYD_H_
	
/***********************************************************************/

namespace pasl {
  namespace graph {
    
    template <class Adjlist_seq>
    class floyd_algo: public base_algo<Adjlist_seq> {
    public:
      
      std::string get_impl_name(int index) {
        return fw_algo_names[index];
      }
      
      int get_impl_count() {
        return FW_NB_ALGO;
      }
      
      int* get_dist(int algo_id, const adjlist<Adjlist_seq>& graph, int source) {
        switch (algo_id) {
          case FW_SERIAL_CLASSIC:
            return floyd_seq_classic(graph);
            break;
          case FW_PAR_CLASSIC:
            return floyd_par_classic(graph);
            break;
          case FW_PAR_BFS:
            return floyd_par_bfs(graph);
            break;
          default:
            return floyd_seq_classic(graph);
        }
      }
      
    private:
      enum FW_ALGO { 	
        FW_SERIAL_CLASSIC,
        FW_PAR_CLASSIC,      
        FW_PAR_BFS,
        FW_NB_ALGO 
      };   
      
      std::string const fw_algo_names[3] = {
        "SerialClassic", 
        "ParClassic",       
        "ParBFS"
      };    
      
      /*---------------------------------------------------------------------*/
      /*---------------------------------------------------------------------*/
      /*---------------------------------------------------------------------*/
      /* Floyd-Warshall; serial classic */
      /*---------------------------------------------------------------------*/
      int* floyd_seq_classic(const adjlist<Adjlist_seq>& graph) {
        using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
        int inf_dist = shortest_path_constants<int>::inf_dist;
        vtxid_type nb_vertices = graph.get_nb_vertices();
        int* dists = data::mynew_array<int>(nb_vertices * nb_vertices);
        fill_array_seq(dists, nb_vertices * nb_vertices, inf_dist);
        
        for (size_t i = 0; i < nb_vertices; i++) {
          vtxid_type degree = graph.adjlists[i].get_out_degree();
          for (vtxid_type edge = 0; edge < degree; edge++) {
            vtxid_type other = graph.adjlists[i].get_out_neighbor(edge);
            vtxid_type w = graph.adjlists[i].get_out_neighbor_weight(edge);
            dists[i * nb_vertices + other] = w;
          }
        }
        return dists;
      }
      
      /*---------------------------------------------------------------------*/
      /*---------------------------------------------------------------------*/
      /*---------------------------------------------------------------------*/
      /* Floyd-Warshall; parallel classic */
      /*---------------------------------------------------------------------*/
      int* floyd_par_classic(const adjlist<Adjlist_seq>& graph) {
        using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
        int inf_dist = shortest_path_constants<int>::inf_dist;
        vtxid_type nb_vertices = graph.get_nb_vertices();
        int* dists = data::mynew_array<int>(nb_vertices * nb_vertices);
        fill_array_seq(dists, nb_vertices, inf_dist);
        
        for (size_t i = 0; i < nb_vertices; i++) {
          vtxid_type degree = graph.adjlists[i].get_out_degree();
          for (vtxid_type edge = 0; edge < degree; edge++) {
            vtxid_type other = graph.adjlists[i].get_out_neighbor(edge);
            vtxid_type w = graph.adjlists[i].get_out_neighbor_weight(edge);
            dists[i * nb_vertices + other] = w;
          }
        }
        return dists;
      }
      
      /*---------------------------------------------------------------------*/
      /*---------------------------------------------------------------------*/
      /*---------------------------------------------------------------------*/
      /* Floyd-Warshall; parallel bfs */
      /*---------------------------------------------------------------------*/
      int* floyd_par_bfs(const adjlist<Adjlist_seq>& graph) {
        using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
        int inf_dist = shortest_path_constants<int>::inf_dist;
        vtxid_type nb_vertices = graph.get_nb_vertices();
        int* dists = data::mynew_array<int>(nb_vertices * nb_vertices);
        fill_array_seq(dists, nb_vertices, inf_dist);
        
        for (size_t i = 0; i < nb_vertices; i++) {
          vtxid_type degree = graph.adjlists[i].get_out_degree();
          for (vtxid_type edge = 0; edge < degree; edge++) {
            vtxid_type other = graph.adjlists[i].get_out_neighbor(edge);
            vtxid_type w = graph.adjlists[i].get_out_neighbor_weight(edge);
            dists[i * nb_vertices + other] = w;
          }
        }
        return dists;
      }

    };
          
  } // end namespace graph
} // end namespace pasl

/***********************************************************************/

#endif /*! _PASL_GRAPH_BFS_H_ */
