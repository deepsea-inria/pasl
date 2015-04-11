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
    extern int floyd_warshall_par_serial_cutoff;
    extern int floyd_warshall_par_bfs_cutoff;
    
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
            return floyd_warshall_par_bfs(graph);
            break;
          case FW_PAR_BFS2:
            return floyd_warshall_par_bfs2(graph);
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
        FW_PAR_BFS2,        
        FW_NB_ALGO 
      };   
      
      std::string const fw_algo_names[4] = {
        "SerialClassic", 
        "ParClassic",      
        "ParBFSForAllVertices",
        "ParBFSForEveryVertex"
      };    
      
      static void print_dist(int nb_vertices, int * dist) {
        std::cout << "Distances : " << std::endl;
        int cur = 0;
        for (int i = 0; i < nb_vertices; i++) {
          for (int j = 0; j < nb_vertices; j++) {
            std::cout << dist[cur++] << " ";
          }          
          std::cout << std::endl;
        }
      }
      
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
          dists[i * nb_vertices + i] = 0;
          vtxid_type degree = graph.adjlists[i].get_out_degree();
          for (vtxid_type edge = 0; edge < degree; edge++) {
            vtxid_type other = graph.adjlists[i].get_out_neighbor(edge);
            vtxid_type w = graph.adjlists[i].get_out_neighbor_weight(edge);
            dists[i * nb_vertices + other] = w;
          }
        }
        
        for (int k = 0; k < nb_vertices; k++) {
          for (int i = 0; i < nb_vertices; i++) {
            for (int j = 0; j < nb_vertices; j++) {
              dists[i * nb_vertices + j] = std::min(dists[i * nb_vertices + j], 
                                                    dists[i * nb_vertices + k] + dists[k * nb_vertices + j]);
            }
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
        return floyd_seq_classic(graph);
      }
      
      /*---------------------------------------------------------------------*/
      /*---------------------------------------------------------------------*/
      /*---------------------------------------------------------------------*/
      /* Floyd-Warshall; parallel bfs */
      /* One bfs for all the vertices */      
      /*---------------------------------------------------------------------*/
      static adjlist<Adjlist_seq> modify_graph(const adjlist<Adjlist_seq>& graph) {
        // TODO: make it smarter
        
        using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
        using edge_type = wedge<vtxid_type>;
        using edgelist_bag_type = pasl::data::array_seq<edge_type>;
        using edgelist_type = edgelist<edgelist_bag_type>;
        vtxid_type nb_vertices = graph.get_nb_vertices();
        
        edgelist_type edg;
        adjlist<Adjlist_seq> adj;
        
        vtxid_type cur = 0;
        edg.edges.alloc(nb_vertices * graph.nb_edges);
        for (vtxid_type from = 0; from < nb_vertices; ++from) {
          for (vtxid_type i = 0; i < nb_vertices; i++) {
            vtxid_type degree = graph.adjlists[i].get_out_degree();
            for (vtxid_type edge = 0; edge < degree; edge++) {
              vtxid_type other = graph.adjlists[i].get_out_neighbor(edge);
              vtxid_type w = graph.adjlists[i].get_out_neighbor_weight(edge);
              edg.edges[cur++] = edge_type(from * nb_vertices + i, from * nb_vertices + other, w);
            }
          }
        }
        
        edg.nb_vertices = nb_vertices * nb_vertices;
        edg.check();
        adjlist_from_edgelist(edg, adj);
        return adj;
      }
      
      static int* floyd_warshall_par_bfs(const adjlist<Adjlist_seq>& init_graph) {
        using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
        vtxid_type nb_vertices = init_graph.get_nb_vertices();
        auto graph = modify_graph(init_graph);
        std::vector<vtxid_type> sources;
        for (int i = 0; i < nb_vertices; ++i) {
          sources.push_back(i * nb_vertices + i);
        }
        return bellman_ford_algo<Adjlist_seq>::bfs_bellman_ford::bellman_ford_par_bfs(graph, sources);
      }   
      
      /*---------------------------------------------------------------------*/
      /*---------------------------------------------------------------------*/
      /*---------------------------------------------------------------------*/
      /* Floyd-Warshall; parallel bfs */
      /* Bfs for every vertex */      
      /*---------------------------------------------------------------------*/          
      static int* floyd_warshall_par_bfs2(const adjlist<Adjlist_seq>& init_graph) {
        using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
        vtxid_type nb_vertices = init_graph.get_nb_vertices();
        int* dists = data::mynew_array<int>(nb_vertices * nb_vertices);
        for (int i = 0; i < nb_vertices; ++i) {
          vtxid_type* dist = bellman_ford_algo<Adjlist_seq>::bfs_bellman_ford::bellman_ford_par_bfs(init_graph, i, false);
          for (int j = 0; j < nb_vertices; j++) {
            dists[i * nb_vertices + j] = dist[j];
          }
          free(dist);
        }
        return dists;
      }  
      
    };
    
  } // end namespace graph
} // end namespace pasl

/***********************************************************************/

#endif /*! _PASL_GRAPH_BFS_H_ */
